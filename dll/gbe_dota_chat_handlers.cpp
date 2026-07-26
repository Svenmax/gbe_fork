/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

// Chat-channel and practice-lobby broadcast-channel request handlers for the
// Dota Game Coordinator. Extracted from gbe_dota_handlers.cpp (Phase 3.1.3) to
// group all chat/broadcast request handling into one domain file.
//
// Responsibility boundary: owns chat channel join/leave/message relay, and
// practice-lobby broadcast channel join/update/close. Side-effect ownership and
// ordering are unchanged from the prior monolithic handler file; only the file
// location moved. The single chat-only static helper GBE_GenerateDotaChatChannelId
// moved with the handlers; no cross-TU symbols needed externalization.

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_payload_wire_helpers.h"
#include "gbe_dota_protocol_assets.h"
#include "gbe_dota_gc_diagnostics.h"
#include "gbe_dota_payload_lobby_helpers.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

using GBE_DotaJoinChatChannelRequest = gbe::proto_wire::DotaJoinChatChannelRequest;
using GBE_DotaLeaveChatChannelRequest = gbe::proto_wire::DotaLeaveChatChannelRequest;
using GBE_DotaChatMessageRequest = gbe::proto_wire::DotaChatMessageRequest;
using GBE_DotaPracticeLobbyBroadcastChannelRequest = gbe::proto_wire::DotaPracticeLobbyBroadcastChannelRequest;

// ============================================================================
// Side-effect order documentation (see dll/gbe_dota_action_model.h for the
// canonical action type and cross-domain ordering invariants).
// ============================================================================
//
// GBE_HandleDotaJoinChatChannelRequest (emsg 7009 -> 7010):
//   1. Parse: channel_name (field 1), channel_type (field 2, optional)
//   2. If no local lobby: early return (no side effects)
//   3. Mutate GBE_local_lobby: has_chat_channel=true, chat_channel_id
//      (generate if 0), chat_channel_name, chat_channel_type [coordinator]
//   4. If generic_lobby_id != 0: RefreshLobbyCallbacksForDota() [coordinator]
//   5. GBE_CaptureCurrentDotaLobbyState -> lobby_snapshot [coordinator read]
//   6. GBE_SyncCapturedDotaLobbyState host-only publish [coordinator]
//   7. Build 7010 payload (pure: GBE_AdaptDotaJoinChatChannelResponsePayload)
//   8. GBE_PushDotaResponse(7010) [coordinator: push_incoming_now]
//   9. Log
//   Invariant: lobby mutation precedes capture, host publish, and response.
//
// GBE_HandleDotaChatMessageRequest (emsg 7273, outgoing):
//   1. Parse: text (field 1), channel_id (field 2), account_id (field 3),
//      persona_name (field 4)
//   2. If no local lobby: early return
//   3. Resolve channel_id/account_id/persona_name (defaults from request or
//      settings) [coordinator read]
//   4. Build 7273 chat payload (pure: build_dota_chat_message_payload)
//   5. If network && generic_lobby_id != 0:
//      network->sendToAll(Steam_Messages{FRIEND_CHAT}) [network broadcast]
//   6. Log
//   Note: no local GC queue push (local echo suppressed); relay is network-only.
//
// GBE_HandleDotaNetworkChatMessage (incoming 7273 from network):
//   1. Validate msg, gc_initialized, gc_profile==DOTA2, lobby active
//   2. Extract inner_emsg from message, validate == GBE_kDotaChatMessage
//   3. Parse request (pure)
//   4. Rewrite channel_id varint to local channel (pure: rewrite_varint_fields)
//   5. Resolve sender_name [coordinator read: settings/steam_client/members]
//   6. If sender_name resolved: append field 3 (pure: append_bytes_field)
//   7. Build local_channel_message (pure: build_dota_zero_header_payload)
//   8. push_incoming_now(7273 | proto_mask) [coordinator]
//   9. Log
//   Invariant: local channel rewrite happens before push_incoming_now so the
//   client sees the local channel id, not the remote sender's channel id.
//
// GBE_HandleDotaLeaveChatChannelRequest (emsg 7272 -> 7014, plus 7010 in
// postgame-signout path):
//   1. Parse: channel_id (field 1, optional)
//   2. Compute leave-chat decision (pure: compute_leave_chat_decision)
//   3. If !active || !has_chat_channel (stale path):
//      a. If channel_id == 0: early return
//      b. Build 7014 stale payload (pure)
//      c. GBE_PushDotaResponse(7014) [coordinator]
//      d. return
//   4. If channel_id == 0: early return
//   5. If leaving_legacy_channel_after_signout:
//      a. Build 7010 postgame payload (pure)
//      b. GBE_PushDotaResponse(7010) [coordinator]
//   6. If leaving_non_current_channel_during_abandon:
//      Set GBE_pending_dota_abandon_finalize flags [coordinator mutation]
//   7. Build 7014 payload (pure)
//   8. GBE_PushDotaResponse(7014) [coordinator]
//   9. If leaving_postgame_channel:
//      a. If !matches_current_postgame_channel: return
//      b. GBE_UpdateDotaPracticeLobbyLaunchRichPresence [coordinator]
//      c. clear_chat_channel [state helper]
//      d. Build persona_message (GBE_PrepareDotaPersonaStatePeripheralMessage)
//      e. Log (do NOT queue 766 -- rich presence handled via SetRichPresence)
//   10. Else (not postgame):
//       a. If request_matches_local: clear_chat_channel [state helper]
//       b. Else: return (preserve state, stale 7272 for previous game)
//   11. If leaving_postgame_channel && matches_current && !shared_state.valid:
//       a. GBE_LeaveGenericLobby [coordinator]
//       b. clear_local_lobby [state helper]
//       Else: GBE_PublishSharedDotaLobbyState("7272_leave_chat") [publish]
//   12. GBE_MaybeHandleDotaPracticeLobbyKicked [coordinator]
//   13. Log
//   Invariant: response(7014) precedes lobby state clear precedes publish.
//   Stale-path response(7014) is sent without mutating any lobby state.
//
// GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest (emsg 7149 -> 7055):
//   1. Parse: channel (field 1), country_code/description/language_code (opt)
//   2. If no local lobby: early return
//   3. apply_broadcast_channel [state helper]
//   4. GBE_PublishSharedDotaLobbyState("7149_join_broadcast") [publish]
//   5. GBE_SendDotaPracticeLobbyDetailsUpdate [coordinator: pushes details]
//   6. If has_request_job:
//      a. Build 7055 payload (pure)
//      b. GBE_PushDotaResponse(7055) [coordinator]
//   7. Log
//   Invariant: lobby mutation precedes publish precedes details update precedes
//   response.
//
// GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest (emsg 7367):
//   1. Parse: channel (field 1), country_code/description/language_code (opt)
//   2. If no local lobby: early return
//   3. Mutate GBE_local_lobby: has_broadcast_channel=true, broadcast_channel_id,
//      (conditional) broadcast_country_code/description/language_code [coordinator]
//   4. GBE_PublishSharedDotaLobbyState("7367_update_broadcast") [publish]
//   5. GBE_SendDotaPracticeLobbyDetailsUpdate [coordinator]
//   6. Log
//   Invariant: mutation precedes publish precedes details update.
//
// GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest (emsg 8054):
//   1. Parse: channel (field 1)
//   2. If no local lobby: early return
//   3. Mutate GBE_local_lobby: has_broadcast_channel=false, broadcast_channel_id=
//      request.channel, clear country/description/language [coordinator]
//   4. GBE_PublishSharedDotaLobbyState("8054_close_broadcast") [publish]
//   5. GBE_SendDotaPracticeLobbyDetailsUpdate [coordinator]
//   6. Log
//   Invariant: mutation precedes publish precedes details update.
// ============================================================================

namespace {

// Pure leave-chat decision helper.
//
// Reads the request channel id and the local lobby's chat/postgame state,
// returns all derived decision flags needed by the leave-chat handler. Keeping
// this pure lets the handler focus on executing the decision's side effects in
// the documented order without interleaving boolean derivation logic.
struct LeaveChatDecision {
    uint64 channel_id{};                      // resolved request channel (or local)
    uint64 local_channel_id{};                // GBE_local_lobby.chat_channel_id
    uint64 pre_postgame_channel_id{};         // abandon_pre_postgame_chat_channel_id
    bool leaving_postgame_channel{};          // active postgame abandon + has chat + type==18
    bool matches_current_postgame_channel{};  // request channel == local channel
    bool matches_pre_postgame_channel{};      // request channel == pre-postgame channel
    bool leaving_non_current_channel_during_abandon{};  // postgame + !current + pre
    bool leaving_legacy_channel_after_signout{};        // postgame + !current + pre==0
    bool request_matches_local{};             // request channel == local channel
};

inline LeaveChatDecision compute_leave_chat_decision(
    const GBE_DotaLeaveChatChannelRequest &request,
    const GBE_LocalLobby &lobby)
{
    LeaveChatDecision d;
    d.local_channel_id = lobby.chat_channel_id;
    d.channel_id = request.channel_id != 0 ? request.channel_id : d.local_channel_id;
    d.pre_postgame_channel_id = lobby.abandon_pre_postgame_chat_channel_id;
    d.leaving_postgame_channel =
        lobby.abandon_postgame_active &&
        lobby.has_chat_channel &&
        d.local_channel_id != 0 &&
        lobby.chat_channel_type == 18u;
    d.matches_current_postgame_channel = (d.channel_id == d.local_channel_id);
    d.matches_pre_postgame_channel =
        gbe::dota_lobby_state::postgame_chat_tombstone_matches(lobby, d.channel_id);
    d.leaving_non_current_channel_during_abandon =
        d.leaving_postgame_channel &&
        !d.matches_current_postgame_channel &&
        d.matches_pre_postgame_channel;
    d.leaving_legacy_channel_after_signout =
        d.leaving_postgame_channel &&
        !d.matches_current_postgame_channel &&
        !lobby.postgame_chat_tombstone_active;
    d.request_matches_local = (d.channel_id == d.local_channel_id);
    return d;
}

} // anonymous namespace

static uint64 GBE_GenerateDotaChatChannelId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    const uint64 candidate = 0x62E000ull + (generator() & 0x0000000000000FFFull);
    return candidate != 0 ? candidate : 0x62E638ull;
}


bool Steam_Game_Coordinator::GBE_HandleDotaJoinChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7009 because no local lobby is active");
        return true;
    }

    GBE_DotaJoinChatChannelRequest request{};
    if (!gbe::proto_wire::parse_dota_join_chat_channel_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7009 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    const uint64 chat_channel_id = GBE_local_lobby.chat_channel_id != 0 ?
        GBE_local_lobby.chat_channel_id : GBE_GenerateDotaChatChannelId();
    gbe::dota_lobby_state::apply_chat_channel(
        GBE_local_lobby,
        chat_channel_id,
        request.channel_name,
        request.has_channel_type ? request.channel_type : 3u);

    if (GBE_local_lobby.generic_lobby_id != 0) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking)
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
    }

    GBE_LocalLobby lobby_snapshot{};
    // 7009 is the only generic-metadata host-sync path: clients observe locally,
    // while the host publishes the completed Local capture before the 7010 response.
    if (!GBE_CaptureCurrentDotaLobbyState(
            "7009_join_chat",
            lobby_snapshot,
            GBE_DotaLobbyCaptureMode::WithoutSharedRestore))
        lobby_snapshot = GBE_local_lobby;
    GBE_SyncCapturedDotaLobbyState("7009_join_chat", is_server);

    std::string response_7010;
    if (!GBE_AdaptDotaJoinChatChannelResponsePayload(
            settings->get_local_steam_id().ConvertToUint64(),
            lobby_snapshot.generic_lobby_id,
            lobby_snapshot.chat_channel_id,
            lobby_snapshot.chat_channel_name,
            std::string(settings->get_local_name()),
            lobby_snapshot.members,
            lobby_snapshot.owner_steam_id,
            lobby_snapshot.owner_name,
            lobby_snapshot.chat_channel_type,
            response_7010)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7010 payload for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    if (!GBE_PushDotaJoinChatChannelResponse(response_7010, wrapped, outer_session_field_raw, "7009_7010"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Chat channel joined. name=%s channel_id=%llu channel_type=%u members=%zu wrapped=%d",
        GBE_local_lobby.chat_channel_name.c_str(),
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
        GBE_local_lobby.chat_channel_type,
        lobby_snapshot.members.size(),
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaChatMessageRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7273 because no local lobby is active");
        return true;
    }

    GBE_DotaChatMessageRequest request{};
    if (!gbe::proto_wire::parse_dota_chat_message_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7273 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    const uint64 channel_id = request.has_channel_id ? request.channel_id : GBE_local_lobby.chat_channel_id;
    const uint32 account_id = request.has_account_id ? request.account_id : settings->get_local_steam_id().GetAccountID();
    const std::string persona_name = request.has_persona_name ? request.persona_name : std::string(settings->get_local_name());

    std::string chat_7273;
    if (!gbe::gc_message::build_dota_chat_message_payload(request_body, channel_id, account_id, persona_name, chat_7273)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7273 chat payload channel_id=%llu", static_cast<unsigned long long>(channel_id));
        return true;
    }
    (void)wrapped;
    (void)outer_session_field_raw;

    if (network && GBE_local_lobby.generic_lobby_id != 0) {
        auto steam_message = new Steam_Messages();
        steam_message->set_type(Steam_Messages::FRIEND_CHAT);
        steam_message->set_message(chat_7273);

        Common_Message msg{};
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        msg.set_allocated_steam_messages(steam_message);
        network->sendToAll(&msg, true);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Chat message relayed. channel_id=%llu account_id=%u persona=%s text_size=%zu wrapped=%d local_echo=0",
        static_cast<unsigned long long>(channel_id),
        account_id,
        persona_name.c_str(),
        request.text.size(),
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaNetworkChatMessage(Common_Message *msg)
{
    if (!msg || !msg->has_steam_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return false;

    if (msg->steam_messages().type() != Steam_Messages::FRIEND_CHAT)
        return false;

    const std::string &message = msg->steam_messages().message();
    if (message.size() < 8u)
        return false;

    uint32 inner_emsg = 0;
    std::memcpy(&inner_emsg, message.data(), sizeof(inner_emsg));
    if (GBE_GC_MaskedEMsg(inner_emsg) != GBE_kDotaChatMessage)
        return false;

    const size_t body_offset = 8u;
    if (message.size() <= body_offset)
        return false;

    GBE_DotaChatMessageRequest request{};
    if (!gbe::proto_wire::parse_dota_chat_message_body(
            reinterpret_cast<const uint8 *>(message.data() + body_offset),
            message.size() - body_offset,
            request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring malformed network 7273 chat source=%llu size=%zu",
            static_cast<unsigned long long>(msg->source_id()),
            message.size()
        );
        return true;
    }

    std::string local_channel_body;
    if (!gbe::proto_wire::rewrite_varint_fields(
            message.substr(body_offset),
            { 2u },
            GBE_local_lobby.chat_channel_id,
            local_channel_body))
        return false;

    std::string sender_name;
    const uint64 sender_steam_id = msg->source_id();
    uint32 sender_account_id = request.has_account_id ? request.account_id : 0u;
    if (sender_account_id == 0u && sender_steam_id != 0ull)
        sender_account_id = CSteamID((uint64)sender_steam_id).GetAccountID();
    if (!request.has_persona_name) {
        if (sender_steam_id == settings->get_local_steam_id().ConvertToUint64()) {
            sender_name = std::string(settings->get_local_name());
        } else {
            Steam_Client *steam_client = get_steam_client();
            if (steam_client && steam_client->steam_matchmaking && GBE_local_lobby.generic_lobby_id != 0ull) {
                CSteamID generic_lobby((uint64)GBE_local_lobby.generic_lobby_id);
                CSteamID sender_id((uint64)sender_steam_id);
                if (generic_lobby.IsLobby() && sender_id.IsValid()) {
                    const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby, sender_id, GBE_kDotaGenericLobbyMemberNameKey);
                    if (generic_name && generic_name[0] != '\0')
                        sender_name = std::string(generic_name);
                }
            }

            for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
                if (!sender_name.empty())
                    break;
                if (member.steam_id != sender_steam_id && member.account_id != sender_account_id)
                    continue;

                CSteamID sender_id((uint64)(member.steam_id != 0ull ? member.steam_id : sender_steam_id));
                if (steam_client && steam_client->steam_friends) {
                    const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(sender_id);
                    if (friend_name && friend_name[0] != '\0' && std::string(friend_name) != "Unknown User")
                        sender_name = std::string(friend_name);
                }
                break;
            }
        }

        if (!sender_name.empty())
            gbe::proto_wire::append_bytes_field(local_channel_body, 3u, sender_name);
    }

    std::string local_channel_message;
    if (!gbe::gc_message::build_dota_zero_header_payload(GBE_kDotaChatMessage, local_channel_body, local_channel_message))
        return false;

    push_incoming_now(GBE_kDotaChatMessage | GBE_kProtoMask, local_channel_message);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received network 7273 chat source=%llu size=%zu account_id=%u remote_channel=%llu local_channel=%llu text_size=%zu persona=%s",
        static_cast<unsigned long long>(msg->source_id()),
        message.size(),
        sender_account_id,
        static_cast<unsigned long long>(request.channel_id),
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
        request.text.size(),
        (request.has_persona_name ? request.persona_name : sender_name).c_str()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaLeaveChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaLeaveChatChannelRequest request{};
    if (!gbe::proto_wire::parse_dota_leave_chat_channel_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7272 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    const LeaveChatDecision d = compute_leave_chat_decision(request, GBE_local_lobby);
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 lobby_id = GBE_local_lobby.lobby_id;

    if (!GBE_local_lobby.active || !GBE_local_lobby.has_chat_channel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Replying 7014 for stale 7272 after lobby reset without restoring lobby state. request_channel=%llu active=%u has_chat=%u local_channel=%llu",
            static_cast<unsigned long long>(d.channel_id),
            GBE_local_lobby.active ? 1u : 0u,
            GBE_local_lobby.has_chat_channel ? 1u : 0u,
            static_cast<unsigned long long>(d.local_channel_id)
        );

        if (d.channel_id == 0) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring stale 7272 after lobby reset because no request channel is available");
            return true;
        }

        std::string stale_response_7014;
        if (!gbe::gc_message::build_dota_other_left_channel_payload(d.channel_id, settings->get_local_steam_id().ConvertToUint64(), stale_response_7014)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building stale 7014 payload for channel=%llu", static_cast<unsigned long long>(d.channel_id));
            return true;
        }

        if (!GBE_PushDotaOtherLeftChannelResponse(stale_response_7014, wrapped, outer_session_field_raw, "stale_7272_7014"))
            return true;

        return true;
    }
    if (d.channel_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7272 because no chat channel is active");
        return true;
    }

    if (d.leaving_legacy_channel_after_signout) {
        std::string response_7010_postgame;
        if (gbe::gc_message::build_dota_post_game_join_chat_channel_response_payload(
                steam_id,
                d.local_channel_id,
                GBE_local_lobby.chat_channel_name,
                std::string(settings->get_local_name()),
                response_7010_postgame)) {
            if (wrapped && !outer_session_field_raw) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for postgame 7010 after signout channel=%llu", static_cast<unsigned long long>(d.channel_id));
                return true;
            }
            GBE_PushDotaResponse(GBE_kDotaJoinChatChannelResponse, response_7010_postgame, wrapped, outer_session_field_raw, "postgame_7010_after_signout");
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Queued postgame 7010 after normal signout legacy 7272 request_channel=%llu post_channel=%llu lobby_id=%llu",
                static_cast<unsigned long long>(d.channel_id),
                static_cast<unsigned long long>(d.local_channel_id),
                static_cast<unsigned long long>(lobby_id)
            );
        } else {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building postgame 7010 after signout channel=%llu", static_cast<unsigned long long>(d.channel_id));
        }
    }
    if (d.leaving_non_current_channel_during_abandon) {
        GBE_SetPendingDotaAbandonFinalizeAfterOtherLeftChannel(lobby_id);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Handling pre-postgame 7272 during abandon teardown (replying 7014, reset after retrieval). request_channel=%llu current_postgame_channel=%llu pre_postgame_channel=%llu matched_pre=%u lobby_id=%llu",
            static_cast<unsigned long long>(d.channel_id),
            static_cast<unsigned long long>(d.local_channel_id),
            static_cast<unsigned long long>(d.pre_postgame_channel_id),
            d.matches_pre_postgame_channel ? 1u : 0u,
            static_cast<unsigned long long>(lobby_id)
        );
    }

    std::string response_7014;
    if (!gbe::gc_message::build_dota_other_left_channel_payload(d.channel_id, settings->get_local_steam_id().ConvertToUint64(), response_7014)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7014 payload for channel=%llu", static_cast<unsigned long long>(d.channel_id));
        return true;
    }

    if (!GBE_PushDotaOtherLeftChannelResponse(response_7014, wrapped, outer_session_field_raw, "7272_7014"))
        return true;

    if (d.leaving_postgame_channel) {
        if (!d.matches_current_postgame_channel) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Handled non-current 7272 during postgame teardown. request_channel=%llu local_channel=%llu pre_postgame_channel=%llu",
                static_cast<unsigned long long>(request.channel_id),
                static_cast<unsigned long long>(d.local_channel_id),
                static_cast<unsigned long long>(d.pre_postgame_channel_id)
            );
            return true;
        }

        GBE_ResetDotaPracticeLobbyLaunchRichPresenceToServerSetup();

        // During host disconnect from hero selection, the real client can still be unwinding
        // server/game-rules state after postgame chat leaves. Clearing the entire local/generic
        // lobby snapshot here is too early and can race later disconnect teardown.
        gbe::dota_lobby_state::clear_chat_channel(GBE_local_lobby);
        gbe::dota_lobby_state::clear_postgame_chat_tombstone(GBE_local_lobby);

        std::string persona_message;
        if (!GBE_PrepareDotaPersonaStatePeripheralMessage(GBE_kDotaAbandonPersonaStateInitHex, steam_id, lobby_id, persona_message)) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "failed building abandon persona label=7272_init lobby_id=%llu",
                static_cast<unsigned long long>(lobby_id)
            );
        } else {
            // Rich Presence already handled via ISteamFriends::SetRichPresence.
            // Do not push 766 into GC queue -- Dota does not handle it.
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "built abandon persona label=7272_init lobby_id=%llu size=%zu (not queued, using SetRichPresence)",
                static_cast<unsigned long long>(lobby_id),
                persona_message.size()
            );
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Deferred full lobby reset after postgame 7272 to avoid racing disconnect teardown LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
    } else {
        // Only clear chat state if the request channel matches the current local channel.
        // A stale 7272 from a previous game's PostGame channel should not wipe the current
        // lobby's chat state -- doing so causes the next abandon to lose pre_channel context,
        // which prevents the postgame 7272/7014 from triggering ResetGCMemory and the
        // client never sees the score screen.
        if (d.request_matches_local) {
            gbe::dota_lobby_state::clear_chat_channel(GBE_local_lobby);
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignoring stale 7272 for non-current channel; preserving current chat state. request_channel=%llu local_channel=%llu lobby_id=%llu state=%u game_state=%u",
                static_cast<unsigned long long>(d.channel_id),
                static_cast<unsigned long long>(d.local_channel_id),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );
            return true;
        }
    }

    // If shared state was already cleared by the normal signout finalize (GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed),
    // do not re-publish stale local lobby state back into it. Instead, leave the generic lobby and clear local state.
    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if (d.leaving_postgame_channel && d.matches_current_postgame_channel && !shared_lobby.valid) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipping publish after postgame 7272 because shared state was already cleared by signout finalize LobbyID=%llu",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
        // Leave the generic lobby so other members see the lobby destroyed and can clean up.
        GBE_LeaveGenericLobby();
        gbe::dota_lobby_state::clear_local_lobby(GBE_local_lobby);
    } else {
        GBE_PublishSharedDotaLobbyState("7272_leave_chat");
    }
    if (!GBE_MaybeHandleDotaPracticeLobbyKicked("7272_leave_chat")) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Chat channel left. channel=%llu wrapped=%d",
            static_cast<unsigned long long>(d.channel_id),
            wrapped ? 1 : 0
        );
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Chat channel left. channel=%llu wrapped=%d",
        static_cast<unsigned long long>(d.channel_id),
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7149 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyBroadcastChannelRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_join_broadcast_channel_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7149 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    gbe::dota_lobby_state::apply_broadcast_channel(
        GBE_local_lobby,
        request.channel,
        request.has_country_code ? request.country_code : std::string(),
        request.has_description ? request.description : std::string(),
        request.has_language_code ? request.language_code : std::string());
    GBE_PublishSharedDotaLobbyState("7149_join_broadcast");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7149"))
        return true;

    if (has_request_job) {
        std::string response_7055;
        if (!gbe::gc_message::build_dota_practice_lobby_response_payload(request_job_id, true, response_7055)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for 7149 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        if (!GBE_PushDotaPracticeLobbyResponse(response_7055, wrapped, outer_session_field_raw, "7149_7055"))
            return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Broadcast channel joined. channel=%u country=%s description=%s language=%s",
        GBE_local_lobby.broadcast_channel_id,
        GBE_local_lobby.broadcast_country_code.c_str(),
        GBE_local_lobby.broadcast_description.c_str(),
        GBE_local_lobby.broadcast_language_code.c_str()
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7367 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyBroadcastChannelRequest request{};
    if (!gbe::proto_wire::parse_dota_lobby_update_broadcast_channel_info_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7367 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    gbe::dota_lobby_state::patch_broadcast_channel(
        GBE_local_lobby,
        request.channel,
        request.has_country_code,
        request.country_code,
        request.has_description,
        request.description,
        request.has_language_code,
        request.language_code);
    GBE_PublishSharedDotaLobbyState("7367_update_broadcast");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7367"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Broadcast channel info updated. channel=%u country=%s description=%s language=%s",
        GBE_local_lobby.broadcast_channel_id,
        GBE_local_lobby.broadcast_country_code.c_str(),
        GBE_local_lobby.broadcast_description.c_str(),
        GBE_local_lobby.broadcast_language_code.c_str()
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 8054 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyBroadcastChannelRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_close_broadcast_channel_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 8054 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    gbe::dota_lobby_state::clear_broadcast_channel(GBE_local_lobby, request.channel);
    GBE_PublishSharedDotaLobbyState("8054_close_broadcast");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "8054"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Broadcast channel closed. channel=%u",
        request.channel
    );
    return true;
}
