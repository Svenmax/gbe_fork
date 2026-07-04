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
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
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

    GBE_local_lobby.has_chat_channel = true;
    if (GBE_local_lobby.chat_channel_id == 0)
        GBE_local_lobby.chat_channel_id = GBE_GenerateDotaChatChannelId();
    GBE_local_lobby.chat_channel_name = request.channel_name;
    GBE_local_lobby.chat_channel_type = request.has_channel_type ? request.channel_type : 3u;

    if (GBE_local_lobby.generic_lobby_id != 0) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking)
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
    }

    GBE_PublishSharedDotaLobbyState("7009_join_chat");

    GBE_LocalLobby lobby_snapshot{};
    if (!GBE_CaptureCurrentDotaLobbyState("7009_join_chat", lobby_snapshot))
        lobby_snapshot = GBE_local_lobby;

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

    if (!GBE_PushDotaResponse(GBE_kDotaJoinChatChannelResponse, response_7010, wrapped, outer_session_field_raw, "7009_7010"))
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

    const uint64 local_channel_id = GBE_local_lobby.chat_channel_id;
    const uint64 channel_id = request.channel_id != 0 ? request.channel_id : local_channel_id;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint64 pre_postgame_channel_id = GBE_local_lobby.abandon_pre_postgame_chat_channel_id;
    const bool leaving_postgame_channel =
        GBE_local_lobby.abandon_postgame_active &&
        GBE_local_lobby.has_chat_channel &&
        local_channel_id != 0 &&
        GBE_local_lobby.chat_channel_type == 18u;
    const bool matches_current_postgame_channel = channel_id == local_channel_id;
    const bool matches_pre_postgame_channel = pre_postgame_channel_id != 0 && channel_id == pre_postgame_channel_id;
    if (!GBE_local_lobby.active || !GBE_local_lobby.has_chat_channel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Replying 7014 for stale 7272 after lobby reset without restoring lobby state. request_channel=%llu active=%u has_chat=%u local_channel=%llu",
            static_cast<unsigned long long>(channel_id),
            GBE_local_lobby.active ? 1u : 0u,
            GBE_local_lobby.has_chat_channel ? 1u : 0u,
            static_cast<unsigned long long>(local_channel_id)
        );

        if (channel_id == 0) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring stale 7272 after lobby reset because no request channel is available");
            return true;
        }

        std::string stale_response_7014;
        if (!gbe::gc_message::build_dota_other_left_channel_payload(channel_id, settings->get_local_steam_id().ConvertToUint64(), stale_response_7014)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building stale 7014 payload for channel=%llu", static_cast<unsigned long long>(channel_id));
            return true;
        }

        if (!GBE_PushDotaResponse(GBE_kDotaOtherLeftChannel, stale_response_7014, wrapped, outer_session_field_raw, "stale_7272_7014"))
            return true;

        return true;
    }
    if (channel_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7272 because no chat channel is active");
        return true;
    }

    const bool leaving_non_current_channel_during_abandon =
        leaving_postgame_channel &&
        !matches_current_postgame_channel &&
        matches_pre_postgame_channel;
    const bool leaving_legacy_channel_after_signout =
        leaving_postgame_channel &&
        !matches_current_postgame_channel &&
        pre_postgame_channel_id == 0;
    if (leaving_legacy_channel_after_signout) {
        std::string response_7010_postgame;
        if (gbe::gc_message::build_dota_post_game_join_chat_channel_response_payload(
                steam_id,
                local_channel_id,
                GBE_local_lobby.chat_channel_name,
                std::string(settings->get_local_name()),
                response_7010_postgame)) {
            if (wrapped && !outer_session_field_raw) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for postgame 7010 after signout channel=%llu", static_cast<unsigned long long>(channel_id));
                return true;
            }
            GBE_PushDotaResponse(GBE_kDotaJoinChatChannelResponse, response_7010_postgame, wrapped, outer_session_field_raw, "postgame_7010_after_signout");
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Queued postgame 7010 after normal signout legacy 7272 request_channel=%llu post_channel=%llu lobby_id=%llu",
                static_cast<unsigned long long>(channel_id),
                static_cast<unsigned long long>(local_channel_id),
                static_cast<unsigned long long>(lobby_id)
            );
        } else {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building postgame 7010 after signout channel=%llu", static_cast<unsigned long long>(channel_id));
        }
    }
    if (leaving_non_current_channel_during_abandon) {
        GBE_pending_dota_abandon_finalize_after_7014 = true;
        GBE_pending_dota_abandon_finalize_lobby_id = lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Handling pre-postgame 7272 during abandon teardown (replying 7014, reset after retrieval). request_channel=%llu current_postgame_channel=%llu pre_postgame_channel=%llu matched_pre=%u lobby_id=%llu",
            static_cast<unsigned long long>(channel_id),
            static_cast<unsigned long long>(local_channel_id),
            static_cast<unsigned long long>(GBE_local_lobby.abandon_pre_postgame_chat_channel_id),
            matches_pre_postgame_channel ? 1u : 0u,
            static_cast<unsigned long long>(lobby_id)
        );
    }

    std::string response_7014;
    if (!gbe::gc_message::build_dota_other_left_channel_payload(channel_id, settings->get_local_steam_id().ConvertToUint64(), response_7014)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7014 payload for channel=%llu", static_cast<unsigned long long>(channel_id));
        return true;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaOtherLeftChannel, response_7014, wrapped, outer_session_field_raw, "7272_7014"))
        return true;

    if (leaving_postgame_channel) {
        if (!matches_current_postgame_channel) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Handled non-current 7272 during postgame teardown. request_channel=%llu local_channel=%llu pre_postgame_channel=%llu",
                static_cast<unsigned long long>(request.channel_id),
                static_cast<unsigned long long>(local_channel_id),
                static_cast<unsigned long long>(pre_postgame_channel_id)
            );
            return true;
        }

        GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_INIT", "SERVERSETUP", false, false);

        // During host disconnect from hero selection, the real client can still be unwinding
        // server/game-rules state after postgame chat leaves. Clearing the entire local/generic
        // lobby snapshot here is too early and can race later disconnect teardown.
        GBE_local_lobby.has_chat_channel = false;
        GBE_local_lobby.chat_channel_id = 0;
        GBE_local_lobby.chat_channel_name.clear();
        GBE_local_lobby.chat_channel_type = 0;
        GBE_local_lobby.abandon_pre_postgame_chat_channel_id = 0;

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
        const bool request_matches_local = (channel_id == local_channel_id);
        if (request_matches_local) {
            GBE_local_lobby.has_chat_channel = false;
            GBE_local_lobby.chat_channel_id = 0;
            GBE_local_lobby.chat_channel_name.clear();
            GBE_local_lobby.chat_channel_type = 0;
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignoring stale 7272 for non-current channel; preserving current chat state. request_channel=%llu local_channel=%llu lobby_id=%llu state=%u game_state=%u",
                static_cast<unsigned long long>(channel_id),
                static_cast<unsigned long long>(local_channel_id),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );
            return true;
        }
    }

    // If shared state was already cleared by the normal signout finalize (GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed),
    // do not re-publish stale local lobby state back into it. Instead, leave the generic lobby and clear local state.
    if (leaving_postgame_channel && matches_current_postgame_channel && !GBE_shared_dota_lobby_state.valid) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipping publish after postgame 7272 because shared state was already cleared by signout finalize LobbyID=%llu",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
        // Leave the generic lobby so other members see the lobby destroyed and can clean up.
        GBE_LeaveGenericLobby();
        GBE_local_lobby = GBE_LocalLobby{};
    } else {
        GBE_PublishSharedDotaLobbyState("7272_leave_chat");
    }
    if (!GBE_MaybeHandleDotaPracticeLobbyKicked("7272_leave_chat")) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Chat channel left. channel=%llu wrapped=%d",
            static_cast<unsigned long long>(channel_id),
            wrapped ? 1 : 0
        );
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Chat channel left. channel=%llu wrapped=%d",
        static_cast<unsigned long long>(channel_id),
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

    GBE_local_lobby.has_broadcast_channel = true;
    GBE_local_lobby.broadcast_channel_id = request.channel;
    GBE_local_lobby.broadcast_country_code = request.has_country_code ? request.country_code : std::string();
    GBE_local_lobby.broadcast_description = request.has_description ? request.description : std::string();
    GBE_local_lobby.broadcast_language_code = request.has_language_code ? request.language_code : std::string();
    GBE_PublishSharedDotaLobbyState("7149_join_broadcast");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7149"))
        return true;

    if (has_request_job) {
        std::string response_7055;
        if (!gbe::gc_message::build_dota_practice_lobby_response_payload(request_job_id, true, response_7055)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for 7149 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        if (!GBE_PushDotaResponse(GBE_kDotaPracticeLobbyResponse, response_7055, wrapped, outer_session_field_raw, "7149_7055"))
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

    GBE_local_lobby.has_broadcast_channel = true;
    GBE_local_lobby.broadcast_channel_id = request.channel;
    if (request.has_country_code)
        GBE_local_lobby.broadcast_country_code = request.country_code;
    if (request.has_description)
        GBE_local_lobby.broadcast_description = request.description;
    if (request.has_language_code)
        GBE_local_lobby.broadcast_language_code = request.language_code;
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

    GBE_local_lobby.has_broadcast_channel = false;
    GBE_local_lobby.broadcast_channel_id = request.channel;
    GBE_local_lobby.broadcast_country_code.clear();
    GBE_local_lobby.broadcast_description.clear();
    GBE_local_lobby.broadcast_language_code.clear();
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
