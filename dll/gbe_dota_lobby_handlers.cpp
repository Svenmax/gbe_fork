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

// Practice-lobby lifecycle and invite handlers for the Dota Game
// Coordinator. Extracted from gbe_dota_handlers.cpp (Phase 3.1.4) to group all
// lobby create/list/join/leave/launch/set-details/kick/destroy/invite/signout
// handling into one domain file.
//
// Responsibility boundary: owns practice lobby state transitions for the
// 7038/7040/7041/7042/7044/7046/7047/7048/7050/7081/8246 request family, the
// 4512/4513 lobby invite flow, the friend/network lobby invite message intake,
// and the 7035/7004 abandon + signout teardown. Side-effect ownership and
// ordering are unchanged from the prior monolithic handler file; only the file
// location moved. Six lobby-only static helpers moved with the handlers; no
// cross-TU symbols needed externalization (List Y = 0).

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
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

using GBE_DotaPracticeLobbyDetailsRequest = gbe::proto_wire::DotaPracticeLobbyDetailsRequest;
using GBE_DotaPracticeLobbyCreateRequest = gbe::proto_wire::DotaPracticeLobbyCreateRequest;
using GBE_DotaPracticeLobbyJoinRequest = gbe::proto_wire::DotaPracticeLobbyJoinRequest;
using GBE_DotaInviteToLobbyRequest = gbe::proto_wire::DotaInviteToLobbyRequest;
using GBE_DotaLobbyInviteResponseRequest = gbe::proto_wire::DotaLobbyInviteResponseRequest;
using GBE_DotaPracticeLobbySetTeamSlotRequest = gbe::proto_wire::DotaPracticeLobbySetTeamSlotRequest;
using GBE_DotaPracticeLobbyKickRequest = gbe::proto_wire::DotaPracticeLobbyKickRequest;

bool Steam_Game_Coordinator::GBE_PublishDotaPracticeLobbySetDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_PublishDotaPracticeLobbyLocalMemberData("7046_set_details");
    GBE_PublishSharedDotaLobbyState("7046_set_details");
    GBE_PublishDotaPracticeLobbyMetadata("7046_set_details");
    return GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7046");
}


// ============================================================================
// Side-effect order documentation (see dll/gbe_dota_action_model.h for the
// canonical action type and cross-domain ordering invariants).
// ============================================================================
//
// Cross-handler boundary note: this file owns per-handler request
// parsing/mutation/response/side-effect sequencing. Cross-handler lobby state
// machine consolidation (launch/teardown/reconnect transitions sequenced
// across handlers) belongs to Phase 3.4, not here.
//
// GBE_HandleDotaPracticeLobbyCreateRequest (emsg 7038 -> 24 + 7055):
//   1. Parse pre-reset request, apply custom game details (pure)
//   2. compose_create_lobby_reset_plan (pure decision)
//   3. ResetGCMemory("7038_create", true, true) [coordinator]
//   4. If reset_plan.unsubscribe_previous_practice_lobby:
//      push_incoming_now(25, cache unsubscribed) [coordinator]
//   5. Re-parse request, compose_create_lobby_plan (pure) -> GBE_local_lobby
//   6. If custom_game_create: clear reconnect context [coordinator mutation]
//   7. CreateLobbyImmediate (generic lobby) [coordinator + matchmaking]
//   8. PublishDotaPracticeLobbyLocalMemberData + SyncSettingsLobby +
//      PublishSharedDotaLobbyState + PublishDotaPracticeLobbyMetadata [publish]
//   9. Build 24 template (pure: GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay)
//   10. Build 7055 (pure: build_dota_practice_lobby_response_payload)
//   11. RecordDotaLobbyCacheSubscriptionState [coordinator]
//   12. PushDotaResponse(24) [coordinator: push_incoming_now]
//   13. PushDotaResponse(7055) [coordinator]
//   Invariant: reset(25) precedes new-lobby(24) precedes ack(7055).
//   Custom game state is normalized before generic lobby creation.
//
// GBE_HandleDotaLobbyListRequest (emsg 7040 -> 7055 + optional 24/25):
//   1. Compute finishing_leave from pending_leave_after_7040 [coordinator read]
//   2. If !active || lobby_id==0: build 7055 empty-list payload, push, return
//   3. If finishing_leave: push_incoming_now(25) [coordinator], clear pending
//   4. Build 24 cache subscribed (pure)
//   5. Build 7055 lobby list payload (pure)
//   6. push_incoming_now(24) [coordinator]
//   7. push_incoming_now(7055) [coordinator]
//   Invariant: leave-teardown(25) precedes list(24) precedes ack(7055).
//
// GBE_HandleDotaCustomLobbyListRequest (emsg 7046 -> 7055):
//   1. Parse: custom_game_id (field 1)
//   2. Build 7055 custom lobby list payload (pure)
//   3. PushDotaResponse(7055) [coordinator]
//
// GBE_HandleDotaFriendPracticeLobbyListRequest (emsg 7048 -> 7055):
//   1. Build 7055 friend lobby list payload (pure)
//   2. PushDotaResponse(7055) [coordinator]
//
// GBE_HandleDotaPracticeLobbyJoinRequest (emsg 7044 -> 24 + 7113):
//   1. Parse: lobby_id (field 1), pass_key (field 2)
//   2. FindDotaGenericLobbyByDotaLobbyId (coordinator read); if not found and
//      has lobby_id, FindLobbyByDotaLobbyIdForInvite + JoinLobby +
//      RefreshLobbyCallbacksForDota + re-find [coordinator + matchmaking]
//   3. If !matched && !has_lobby_id && !active: generate lobby_id (pure)
//   4. compose_join_lobby_merge_plan (pure) -> GBE_local_lobby
//   5. If matched_generic_lobby: JoinLobby + SyncSettingsLobby [coordinator]
//   6. If has_pass_key: GBE_local_lobby.pass_key = pass_key [coordinator mutation]
//   7. PublishDotaPracticeLobbyLocalMemberData + PublishSharedDotaLobbyState [publish]
//   8. Build 24 (pure: GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed)
//   9. If send_join_response: build 7113 (pure)
//   10. build_outbound_message(24) + build_outbound_message(7113) (pure)
//   11. RecordDotaLobbyCacheSubscriptionState [coordinator]
//   12. push_incoming_now(24) [coordinator]
//   13. If send_join_response: push_incoming_now(7113) [coordinator]
//   Invariant: cache(24) precedes join-ack(7113) when both sent.
//
// GBE_HandleDotaInviteToLobbyRequest (emsg 4512 -> 7055):
//   1. Parse: invitee_steam_id (field 1)
//   2. Build 2011 lobby invite cache subscribed (pure helper)
//   3. Build 7055 ack (pure)
//   4. If generic_lobby_id != 0: network->sendToAll(invite msg) [network broadcast]
//   5. push_incoming_now(2011) [coordinator]
//   6. push_incoming_now(7055) [coordinator]
//   Invariant: invite(2011) precedes ack(7055).
//
// GBE_HandleDotaLobbyInviteResponseRequest (emsg 4513 -> decline: 26 + 25;
// accept: delegate to 7044 + 26 + 25):
//   1. Parse: lobby_id, accept, client_version
//   2. Find matched generic lobby (coordinator read + matchmaking)
//   3. If declined:
//      a. Build remove-2011 (pure)
//      b. If !wrapped || outer_session_field_raw: PushDotaResponse(26, remove-2011)
//      c. Build 25 (pure)
//      d. If !wrapped || outer_session_field_raw: PushDotaResponse(25)
//      e. return
//   4. If accepted:
//      a. Delegate to GBE_HandleDotaPracticeLobbyJoinRequest (emits 24 + 7113)
//      b. Build remove-2011 (pure) + PushDotaResponse(26)
//      c. Build 25 (pure) + PushDotaResponse(25)
//   Invariant: join(24) precedes remove-2011(26) precedes 25 on accept path.
//
// GBE_HandleDotaFriendLobbyInviteMessage (incoming friend_messages network msg):
//   1. Validate msg, gc_initialized, gc_profile==DOTA2
//   2. Parse invite payload from friend_messages.message()
//   3. If invite is for a known generic lobby: JoinLobby + sync [coordinator]
//   4. Build 2011 invite cache subscribed (pure)
//   5. push_incoming_now(2011) [coordinator]
//   Invariant: validate before parse before queue push.
//
// GBE_HandleDotaNetworkLobbyInviteMessage (incoming steam_messages network msg):
//   1. Validate msg, gc_initialized, gc_profile==DOTA2
//   2. Extract inner_emsg, validate == GBE_kDotaPracticeLobbyInvite
//   3. Build 2011 from network payload (pure: GBE_AdaptDotaLobbyInviteCacheSubscribedPayload)
//   4. push_incoming_now(2011) [coordinator]
//
// GBE_HandleDotaAbandonCurrentGameRequest (emsg 7035 -> 25 + postgame 7010):
//   1. Compute abandon decision (pure: compute_abandon_decision)
//   2. If !active || lobby_id==0: early return
//   3. If arcade_launch_failed_before_connect && within grace window: return
//   4. If arcade_launch_failed_before_connect (past grace):
//      a. Build 25 (pure)
//      b. GBE_DiscardQueuedDotaLaunchMessagesForAbandon [coordinator]
//      c. Set GBE_pending_reset_after_cache_unsubscribed flags [coordinator mutation]
//      d. GBE_MarkDotaAbandonedLobbySuppressed [coordinator]
//      e. push_incoming_now(25) [coordinator]
//      f. return (skip postgame)
//   5. If !ready_for_abandon_teardown:
//      a. If treat_as_current_game_disconnect: queue 25 + deferred reset
//      b. Else: log + return
//   6. If ready_for_abandon_teardown:
//      a. GBE_DiscardQueuedDotaLaunchMessagesForAbandon [coordinator]
//      b. GBE_MarkDotaAbandonedLobbySuppressed [coordinator]
//      c. GBE_QueueDotaPostGameTeardown (emits 25 + postgame 7010) [coordinator]
//   Invariant: 25 precedes postgame 7010; arcade-failed path skips postgame.
//   Grace window check must happen before arcade-failed teardown.
//
// GBE_HandleDotaGameMatchSignOutRequest (emsg 7004 -> 7055):
//   1. If !active || lobby_id==0: build 7055 empty, push, return
//   2. Build 7055 signout payload (pure)
//   3. GBE_QueueDotaPostGameTeardown [coordinator: emits 25 + postgame 7010]
//   4. PushDotaResponse(7055) [coordinator]
//   Invariant: postgame teardown precedes ack(7055).
//
// GBE_HandleDotaPracticeLobbyLeaveRequest (emsg 7042 -> 25 + postgame 7010):
//   1. If !active || lobby_id==0: early return
//   2. Build 25 (pure)
//   3. GBE_MarkDotaAbandonedLobbySuppressed [coordinator]
//   4. GBE_QueueDotaPostGameTeardown [coordinator: emits 25 + postgame 7010]
//   Invariant: 25 precedes postgame 7010.
//
// GBE_HandleDotaPracticeLobbyLaunchRequest (emsg 7041 -> 26 + optional 8052):
//   1. If !active || lobby_id==0: early return
//   2. If wrapped && !outer_session_field_raw: early return
//   3. GBE_ResetDotaPracticeLobbyLaunchPeripheralState [coordinator]
//   4. compose_launch_init_plan (pure) -> GBE_local_lobby
//   5. PublishSharedDotaLobbyState("7041_launch_init") [publish]
//   6. If custom_game: compose_launch_serversetup_presence_event (pure) +
//      UpdateRichPresence + MaybeQueuePersonaState [coordinator]
//   7. If custom_game && GBE_SendDotaCustomGameLaunchSetupFlow (emits 8052):
//      return (deferred to 8052)
//   8. compose_practice_lobby_launch_event_plan (pure)
//   9. Build 26 (pure: GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate)
//   10. PushDotaResponse(26) [coordinator]
//   11. If steam_auth_ack.queue: MaybeQueueSteamAuthAck [coordinator]
//   12. If presence.update: UpdateRichPresence + MaybeQueuePersonaState [coordinator]
//   Invariant: launch-init publish precedes 26 details update; custom game
//   path defers to 8052 and skips the 26 details update.
//
// GBE_HandleDotaPracticeLobbySetDetailsRequest (emsg 7050 -> 26 + 7055):
//   1. Parse: lobby details (pass-through to compose_set_details_plan)
//   2. compose_set_details_plan (pure) -> GBE_local_lobby + changed flags
//   3. PublishSharedDotaLobbyState("7050_set_details") [publish]
//   4. GBE_SendDotaPracticeLobbyDetailsUpdate (emits 26) [coordinator]
//   5. If has_request_job: build 7055 + PushDotaResponse(7055) [coordinator]
//   Invariant: state mutation precedes publish precedes 26 precedes 7055.
//
// GBE_HandleDotaPracticeLobbySetTeamSlotRequest (emsg 7047 -> 26 + 7055):
//   1. Parse: team, slot, bot_difficulty
//   2. If local_is_owner: update owner_team/owner_slot [coordinator mutation]
//   3. apply_lobby_member_team_slot_update (pure) -> members
//   4. If has_bot_difficulty: update bot_difficulty_{radiant,dire} [coordinator]
//   5. NormalizeDotaArcadeLobbyMemberSlots [coordinator]
//   6. PublishLocalMemberData + PublishSharedDotaLobbyState [publish]
//   7. GBE_SendDotaPracticeLobbyDetailsUpdate (emits 26) [coordinator]
//   8. If has_request_job: build 7055 + PushDotaResponse(7055) [coordinator]
//   Invariant: mutation precedes publish precedes 26 precedes 7055.
//
// GBE_HandleDotaPracticeLobbyKickRequest (emsg 7081 -> 26 + 7055):
//   1. Parse: target_steam_id
//   2. If !active || generic_lobby_id==0: early return
//   3. Find target member; if not found: build 7055 + push + return
//   4. Remove target from members [coordinator mutation]
//   5. PublishSharedDotaLobbyState [publish]
//   6. GBE_SendDotaPracticeLobbyDetailsUpdate (emits 26) [coordinator]
//   7. If has_request_job: build 7055 + PushDotaResponse(7055) [coordinator]
//   Invariant: member removal precedes publish precedes 26 precedes 7055.
//
// GBE_HandleDotaDestroyLobbyRequest (emsg 8246 -> 25 + 7055):
//   1. If !active || lobby_id==0: build 7055 empty, push, return
//   2. Build 25 (pure)
//   3. push_incoming_now(25) [coordinator]
//   4. GBE_MarkDotaAbandonedLobbySuppressed [coordinator]
//   5. GBE_local_lobby = {} [coordinator mutation]
//   6. GBE_LeaveGenericLobby [coordinator]
//   7. If has_request_job: build 7055 + PushDotaResponse(7055) [coordinator]
//   Invariant: 25 precedes lobby clear precedes generic lobby leave.
// ============================================================================


static void GBE_ApplyDotaCustomGameDetailsRequest(const GBE_DotaPracticeLobbyDetailsRequest &request, GBE_DotaCustomGameDetails &custom_game)
{
    if (request.has_custom_game_mode)
        custom_game.mode = request.custom_game_mode;
    if (request.has_custom_map_name)
        custom_game.map_name = request.custom_map_name;
    if (request.has_custom_difficulty)
        custom_game.difficulty = request.custom_difficulty;
    if (request.has_custom_game_id)
        custom_game.game_id = request.custom_game_id;
    if (request.has_custom_min_players)
        custom_game.min_players = request.custom_min_players;
    if (request.has_custom_max_players)
        custom_game.max_players = request.custom_max_players;
    if (request.has_custom_game_crc)
        custom_game.crc = request.custom_game_crc;
    if (request.has_custom_game_timestamp)
        custom_game.timestamp = request.custom_game_timestamp;
    if (request.has_custom_game_penalties)
        custom_game.penalties = request.custom_game_penalties;
}


static void GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(class Settings *settings, GBE_DotaCustomGameDetails &custom_game)
{
    if (!settings || custom_game.game_id == 0ull || !settings->isModInstalled(static_cast<PublishedFileId_t>(custom_game.game_id)))
        return;

    Mod_entry mod = settings->getMod(static_cast<PublishedFileId_t>(custom_game.game_id));
    const std::string addon_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "addon_name", mod.title);
    const std::string metadata_map_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "map_name", addon_name);

    if (gbe::proto_wire::dota_is_readable_custom_game_name(addon_name) && (custom_game.mode.empty() || gbe::proto_wire::dota_string_is_unsigned_integer(custom_game.mode)))
        custom_game.mode = addon_name;
    if (gbe::proto_wire::dota_is_readable_custom_game_name(metadata_map_name) && (custom_game.map_name.empty() || custom_game.map_name == "dota" || gbe::proto_wire::dota_string_is_unsigned_integer(custom_game.map_name)))
        custom_game.map_name = metadata_map_name;
}


static uint64 GBE_GenerateDotaLobbyId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 64; ++attempt) {
        const uint64 candidate = (generator() & 0x00FFFFFFFFFFFFFFull) | 0x0002000000000000ull;
        std::vector<uint8> encoded;
        if (candidate != 0 && gbe::proto_wire::encode_varuint_with_expected_size(candidate, GBE_kOldDotaLobbyIdVarint.size(), encoded))
            return candidate;
    }

    return 29799760111995806ull;
}


static uint64 GBE_GenerateDotaMatchId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 128; ++attempt) {
        const uint64 candidate = 0x100000000ull + (generator() & 0x00000003FFFFFFFFull);
        std::vector<uint8> encoded;
        if (candidate != 0 && gbe::proto_wire::encode_varuint_with_expected_size(candidate, GBE_kOldDotaPracticeLobbyMatchIdVarint.size(), encoded))
            return candidate;
    }

    return 8781757536ull;
}


static bool GBE_AdaptDotaLobbyInviteCacheSubscribedPayload(
    uint64 lobby_id,
    uint64 inviter_steam_id,
    uint64 invitee_steam_id,
    const std::string &inviter_name,
    const std::vector<std::pair<uint64, std::string>> &members,
    std::string &message)
{
    uint64 cache_version = 0;
    uint64 invite_gid = 0;
    std::vector<std::pair<std::uint64_t, std::string>> parsed_members;
    parsed_members.reserve(members.size());
    for (const auto &member : members)
        parsed_members.push_back({ member.first, member.second });
    std::uint64_t parsed_invite_gid = 0;
    std::uint64_t parsed_cache_version = 0;
    if (!gbe::gc_message::build_dota_lobby_invite_cache_subscribed_payload(lobby_id, inviter_steam_id, invitee_steam_id, inviter_name, parsed_members, message, &parsed_invite_gid, &parsed_cache_version))
        return false;
    invite_gid = static_cast<uint64>(parsed_invite_gid);
    cache_version = static_cast<uint64>(parsed_cache_version);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Built 2011 lobby invite lobby_id=%llu invitee=%llu inviter=%llu invite_gid=%llu cache_version=%llu members=%zu",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(invitee_steam_id),
        static_cast<unsigned long long>(inviter_steam_id),
        static_cast<unsigned long long>(invite_gid),
        static_cast<unsigned long long>(cache_version),
        members.size());
    return true;
}


static bool GBE_IsDotaLobbyInviteCacheSubscribedPayload(const std::string &message)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return false;

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        if (object.type_id() == 2011 && object.object_data_size() > 0)
            return true;
    }

    return false;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyCreateRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaPracticeLobbyCreateRequest pre_reset_request{};
    GBE_DotaCustomGameDetails pre_reset_custom_game{};
    const bool parsed_pre_reset_request = gbe::proto_wire::parse_dota_practice_lobby_create_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), pre_reset_request);
    if (parsed_pre_reset_request && pre_reset_request.has_lobby_details) {
        GBE_ApplyDotaCustomGameDetailsRequest(pre_reset_request.lobby_details, pre_reset_custom_game);
        GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, pre_reset_custom_game);
    }

    const gbe::dota_lobby_state::CreateLobbyResetPlan reset_plan = gbe::dota_lobby_state::compose_create_lobby_reset_plan(GBE_local_lobby, pre_reset_custom_game);
    const GBE_DotaActionList create_actions = gbe::dota_lobby_flow::create_lobby_action_list(
        gbe::dota_lobby_flow::CreateLobbyActionPlan{reset_plan.unsubscribe_previous_practice_lobby},
        wrapped);
    std::size_t create_action_index = 0u;

    ResetGCMemory("7038_create", true, true);

    if (create_action_index < create_actions.size() &&
            create_actions[create_action_index].type == GBE_DotaActionType::PushIncomingNow &&
            create_actions[create_action_index].emsg == (GBE_kDotaCacheUnsubscribed | GBE_kProtoMask)) {
        std::string response_25;
        if (gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(reset_plan.previous_lobby_id, response_25)) {
            push_incoming_now(create_actions[create_action_index].emsg, response_25);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Unsubscribed previous practice lobby before arcade create previous_lobby_id=%llu previous_match_id=%llu previous_state=%u previous_game_state=%u previous_team=%u previous_slot=%u custom_game_id=%llu size=%zu",
                static_cast<unsigned long long>(reset_plan.previous_lobby_id),
                static_cast<unsigned long long>(reset_plan.previous_match_id),
                reset_plan.previous_state,
                reset_plan.previous_game_state,
                reset_plan.previous_owner_team,
                reset_plan.previous_owner_slot,
                static_cast<unsigned long long>(pre_reset_custom_game.game_id),
                response_25.size()
            );
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Failed unsubscribing previous practice lobby before arcade create previous_lobby_id=%llu custom_game_id=%llu",
                static_cast<unsigned long long>(reset_plan.previous_lobby_id),
                static_cast<unsigned long long>(pre_reset_custom_game.game_id)
            );
        }
        ++create_action_index;
    }

    GBE_DotaPracticeLobbyCreateRequest request{};
    const bool parsed_create_request = gbe::proto_wire::parse_dota_practice_lobby_create_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request);
    const gbe::dota_lobby_state::CreateLobbyPlan create_plan = gbe::dota_lobby_state::compose_create_lobby_plan(
        request,
        GBE_GenerateDotaLobbyId(),
        settings->get_local_steam_id().ConvertToUint64(),
        settings->get_local_steam_id().GetAccountID(),
        std::string(settings->get_local_name()),
        GBE_kDotaTeamGoodGuys,
        1u);
    GBE_local_lobby = create_plan.lobby;
    if (parsed_create_request && request.has_lobby_details) {
        GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, GBE_local_lobby.custom_game);
        const bool custom_game_create = GBE_local_lobby.custom_game.game_id != 0ull;
        GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby);
        if (custom_game_create) {
            GBE_ClearRecentDotaReconnectContext();
            GBE_SetDotaReconnectEligible(true);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Isolated arcade lobby from prior practice runtime lobby_id=%llu custom_game_id=%llu",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id)
            );
        }
    }

    Steam_Client *steam_client = get_steam_client();
    if (steam_client && steam_client->steam_matchmaking) {
        CSteamID generic_lobby_id = steam_client->steam_matchmaking->CreateLobbyImmediate(k_ELobbyTypeInvisible, 10);
        if (generic_lobby_id.IsLobby())
            GBE_local_lobby.generic_lobby_id = generic_lobby_id.ConvertToUint64();
    }

    for (; create_action_index < create_actions.size(); ++create_action_index) {
        const GBE_DotaAction &action = create_actions[create_action_index];
        if (action.type == GBE_DotaActionType::LobbyCacheSubscriptionRecord)
            break;
        switch (action.type) {
            case GBE_DotaActionType::LobbyLocalMemberData:
                GBE_PublishDotaPracticeLobbyLocalMemberData(action.reason.c_str());
                break;
            case GBE_DotaActionType::SettingsLobbySync:
                GBE_SyncSettingsLobbyFromGenericLobby(action.reason.c_str());
                break;
            case GBE_DotaActionType::LobbySnapshotRefresh:
                GBE_PublishSharedDotaLobbyState(action.reason.c_str());
                break;
            case GBE_DotaActionType::LobbyMetadataPublish:
                GBE_PublishDotaPracticeLobbyMetadata(action.reason.c_str());
                break;
            default:
                break;
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] State creating path=%s has_job=%u request_job=%llu NewLobbyID=%llu GenericLobbyID=%llu room=%s server_region=%u lan=%u lan_ping=%s mode=%u pass_len=%zu custom_id=%llu custom_mode=%s custom_map=%s custom_min=%u custom_max=%u",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan ? 1u : 0u,
        GBE_local_lobby.lan_host_ping_location.c_str(),
        GBE_local_lobby.game_mode,
        GBE_local_lobby.pass_key.size(),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
        GBE_local_lobby.custom_game.mode.c_str(),
        GBE_local_lobby.custom_game.map_name.c_str(),
        GBE_local_lobby.custom_game.min_players,
        GBE_local_lobby.custom_game.max_players
    );

    std::string response_24;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    if (!GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building template 24 cache update for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string response_7055;
    if (!gbe::gc_message::build_dota_practice_lobby_response_payload(request_job_id, has_request_job, response_7055)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string wrapped_24;
    std::string wrapped_7055;
    for (; create_action_index < create_actions.size(); ++create_action_index) {
        const GBE_DotaAction &action = create_actions[create_action_index];
        if (action.type == GBE_DotaActionType::PushIncomingNow &&
                action.emsg == (GBE_kDotaPracticeLobbyResponse | GBE_kProtoMask))
            break;
        switch (action.type) {
            case GBE_DotaActionType::LobbyCacheSubscriptionRecord:
                GBE_RecordDotaLobbyCacheSubscriptionState(response_24, action.reason.c_str());
                break;
            case GBE_DotaActionType::PushIncomingNow:
                if (action.emsg == (GBE_kDotaCacheSubscribed | GBE_kProtoMask)) {
                    if (!GBE_PushDotaResponse(GBE_kDotaCacheSubscribed, response_24, wrapped, outer_session_field_raw, action.reason.c_str(), false, 0u, 0u, &wrapped_24))
                        return true;
                }
                break;
            default:
                break;
        }
    }

    if (wrapped) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 24 cache update with NewLobbyID=%llu size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            wrapped_24.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_24.data()), response_24.size(), 32).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(wrapped_24.data()), wrapped_24.size(), 32).c_str()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 24 cache update with NewLobbyID=%llu size=%zu body_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            response_24.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_24.data()), response_24.size(), 32).c_str()
        );
    }

    if (create_action_index < create_actions.size()) {
        const GBE_DotaAction &action = create_actions[create_action_index];
        if (action.type == GBE_DotaActionType::PushIncomingNow &&
                action.emsg == (GBE_kDotaPracticeLobbyResponse | GBE_kProtoMask)) {
            if (!GBE_PushDotaPracticeLobbyResponse(response_7055, wrapped, outer_session_field_raw, action.reason.c_str(), &wrapped_7055))
                return true;
            ++create_action_index;
        }
    }

    if (wrapped) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 7055 with NewLobbyID=%llu has_job=%u request_job=%llu size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            has_request_job ? 1u : 0u,
            static_cast<unsigned long long>(request_job_id),
            wrapped_7055.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(wrapped_7055.data()), wrapped_7055.size(), 32).c_str()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 7055 with NewLobbyID=%llu has_job=%u request_job=%llu size=%zu body_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            has_request_job ? 1u : 0u,
            static_cast<unsigned long long>(request_job_id),
            response_7055.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str()
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Skipping initial 26 details update for 7038 to match official create flow LobbyID=%llu",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
    );

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLobbyListRequest(bool has_request_job, uint64 request_job_id, bool wrapped, const std::string *outer_session_field_raw)
{
    const bool finishing_leave = GBE_local_lobby.pending_leave_after_7040 && GBE_local_lobby.pending_leave_lobby_id != 0;
    const uint64 leaving_lobby_id = GBE_local_lobby.pending_leave_lobby_id;

    std::string response_25;
    if (finishing_leave && !gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(leaving_lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building delayed 25 for 7040 LobbyID=%llu", static_cast<unsigned long long>(leaving_lobby_id));
        return true;
    }

    std::vector<GBE_LocalLobby> lobby_snapshots;
    if (!finishing_leave)
        lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("8011_lobby_list");
    if (!finishing_leave && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        const uint64 local_lobby_id = GBE_local_lobby.lobby_id;
        const bool already_included = std::any_of(lobby_snapshots.begin(), lobby_snapshots.end(), [local_lobby_id](const GBE_LocalLobby &snapshot) {
            return snapshot.lobby_id == local_lobby_id;
        });
        if (!already_included)
            lobby_snapshots.push_back(GBE_local_lobby);
    }

    std::vector<std::string> entries;
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        if (GBE_ShouldSuppressDotaAbandonedLobby(snapshot.lobby_id))
            continue;
        entries.push_back(gbe::gc_message::build_dota_practice_lobby_list_entry_body(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            snapshot.room_name,
            snapshot.game_mode,
            snapshot.server_region,
            !snapshot.pass_key.empty(),
            1u,
            10u,
            snapshot.lan_host_ping_location));
    }

    std::string response_8012;
    if (!gbe::gc_message::build_dota_lobby_list_response_payload(entries, response_8012)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 8012 lobby list response active=%u finishing_leave=%u", GBE_local_lobby.active ? 1u : 0u, finishing_leave ? 1u : 0u);
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 8012 lobby list");
        return true;
    }

    if (finishing_leave) {
        GBE_PushDotaCacheUnsubscribedResponse(response_25, wrapped, outer_session_field_raw, "7040_leave_after_lobby_list_25");
        ResetGCMemory("7040_leave_after_lobby_list", true, false);
    }

    if (!GBE_PushDotaResponse(GBE_kDotaLobbyListResponse, response_8012, wrapped, outer_session_field_raw, "8012_lobby_list"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent lobby list response 8012 entries=%zu finishing_leave=%u request_job=%llu has_job=%u wrapped=%d",
        entries.size(),
        finishing_leave ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1u : 0u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaCustomLobbyListRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    const uint8 *request_data = reinterpret_cast<const uint8 *>(request_body.data());
    uint64 request_list_job_id = 0ull;
    std::string requested_pass_key;
    gbe::proto_wire::read_uint64_field(request_data, request_body.size(), 10u, request_list_job_id);
    gbe::proto_wire::read_bytes_field(request_data, request_body.size(), 2u, requested_pass_key);

    std::vector<GBE_LocalLobby> lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("7042_custom_lobby_list");
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0ull) {
        const uint64 local_lobby_id = GBE_local_lobby.lobby_id;
        const bool already_included = std::any_of(lobby_snapshots.begin(), lobby_snapshots.end(), [local_lobby_id](const GBE_LocalLobby &snapshot) {
            return snapshot.lobby_id == local_lobby_id;
        });
        if (!already_included)
            lobby_snapshots.push_back(GBE_local_lobby);
    }

    std::vector<uint64> seen_lobby_ids;
    std::vector<std::string> entries;
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        if (!snapshot.active || snapshot.lobby_id == 0ull || snapshot.custom_game.game_id == 0ull)
            continue;
        if (GBE_ShouldSuppressDotaAbandonedLobby(snapshot.lobby_id))
            continue;
        if (!requested_pass_key.empty() && snapshot.pass_key != requested_pass_key)
            continue;
        if (std::find(seen_lobby_ids.begin(), seen_lobby_ids.end(), snapshot.lobby_id) != seen_lobby_ids.end())
            continue;

        entries.push_back(gbe::gc_message::build_dota_custom_lobby_list_entry_body(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            !snapshot.pass_key.empty(),
            snapshot.lan_host_ping_location));
        seen_lobby_ids.push_back(snapshot.lobby_id);
    }

    std::string response_7043;
    if (!gbe::gc_message::build_dota_custom_lobby_list_response_payload(request_list_job_id, entries, response_7043)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7043 custom lobby list response entries=%zu", entries.size());
        return true;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaCustomLobbyListResponse, response_7043, wrapped, outer_session_field_raw, "7043_custom_lobby_list"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent custom lobby list response 7043 entries=%zu request_list_job=%llu pass_key_filter=%u wrapped=%d",
        entries.size(),
        static_cast<unsigned long long>(request_list_job_id),
        requested_pass_key.empty() ? 0u : 1u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaFriendPracticeLobbyListRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    std::vector<std::string> entries;
    const std::vector<GBE_LocalLobby> lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("7111_friend_lobby_list");
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        if (GBE_ShouldSuppressDotaAbandonedLobby(snapshot.lobby_id))
            continue;
        entries.push_back(gbe::gc_message::build_dota_practice_lobby_list_entry_body(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            snapshot.room_name,
            snapshot.game_mode,
            snapshot.server_region,
            !snapshot.pass_key.empty(),
            1u,
            10u,
            snapshot.lan_host_ping_location));
    }

    std::string response_7112;
    if (!gbe::gc_message::build_dota_friend_practice_lobby_list_response_payload(entries, response_7112)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7112 friend practice lobby list response");
        return true;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaFriendPracticeLobbyListResponse, response_7112, wrapped, outer_session_field_raw, "7112_friend_lobby_list"))
        return true;

    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Sent friend practice lobby list response 7112 entries=%zu wrapped=%d", entries.size(), wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyJoinRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw, bool send_join_response)
{
    GBE_DotaPracticeLobbyJoinRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_join_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7044 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    CSteamID matched_generic_lobby_id = k_steamIDNil;
    GBE_LocalLobby matched_lobby{};
    bool matched_generic_lobby = request.has_lobby_id && request.lobby_id != 0 &&
        GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "7044_join");

    Steam_Client *steam_client = get_steam_client();
    if (!matched_generic_lobby && steam_client && steam_client->steam_matchmaking && request.has_lobby_id && request.lobby_id != 0) {
        matched_generic_lobby_id = steam_client->steam_matchmaking->FindLobbyByDotaLobbyIdForInvite(
            request.lobby_id,
            GBE_kDotaGenericLobbyMarkerKey,
            GBE_kDotaGenericLobbyMarkerValue,
            GBE_kDotaGenericLobbyDotaLobbyIdKey);
        if (matched_generic_lobby_id.IsLobby()) {
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
            matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "7044_join_local_find");
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] 7044 local find fallback lobby_id=%llu generic_lobby_id=%llu matched=%u members=%zu",
                static_cast<unsigned long long>(request.lobby_id),
                static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
                matched_generic_lobby ? 1u : 0u,
                matched_lobby.members.size()
            );
        }
    }

    if (!matched_generic_lobby && (!request.has_lobby_id || request.lobby_id == 0) && (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0))
        matched_lobby.lobby_id = GBE_GenerateDotaLobbyId();

    const bool join_has_lobby_id = request.has_lobby_id || matched_lobby.lobby_id != 0ull;
    const uint64 join_lobby_id = request.has_lobby_id && request.lobby_id != 0 ? request.lobby_id : matched_lobby.lobby_id;
    const gbe::dota_lobby_state::JoinLobbyMergePlan join_plan = gbe::dota_lobby_state::compose_join_lobby_merge_plan(
        GBE_local_lobby,
        join_has_lobby_id,
        join_lobby_id,
        matched_generic_lobby,
        matched_lobby,
        settings->get_local_steam_id().ConvertToUint64(),
        settings->get_local_steam_id().GetAccountID(),
        std::string(settings->get_local_name()),
        GBE_kDotaTeamGoodGuys,
        GBE_kDotaTeamPlayerPool);
    GBE_local_lobby = join_plan.lobby;

    if (matched_generic_lobby) {
        if (steam_client && steam_client->steam_matchmaking)
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
        GBE_SyncSettingsLobbyFromGenericLobby("7044_join_generic");
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7044 matched generic custom metadata lobby_id=%llu generic_lobby_id=%llu custom_game_id=%llu custom_mode=%s custom_map=%s state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
            static_cast<unsigned long long>(matched_lobby.lobby_id),
            static_cast<unsigned long long>(matched_lobby.generic_lobby_id),
            static_cast<unsigned long long>(matched_lobby.custom_game.game_id),
            matched_lobby.custom_game.mode.c_str(),
            matched_lobby.custom_game.map_name.c_str(),
            matched_lobby.state,
            matched_lobby.game_state,
            static_cast<unsigned long long>(matched_lobby.match_id),
            static_cast<unsigned long long>(matched_lobby.server_id),
            matched_lobby.connect.c_str());
    }

    const GBE_DotaLobbyMemberState &local_member = join_plan.local_member;

    if (request.has_pass_key)
        GBE_local_lobby.pass_key = request.pass_key;
    GBE_PublishDotaPracticeLobbyLocalMemberData("7044_join");
    GBE_PublishSharedDotaLobbyState("7044_join");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Preparing 7044 cache update LobbyID=%llu owner_steam=%llu local_steam=%llu members=%zu matched_generic_members=%zu custom_game_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        static_cast<unsigned long long>(local_member.steam_id),
        GBE_local_lobby.members.size(),
        matched_lobby.members.size(),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_local_lobby.connect.c_str()
    );

    std::string response_24;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(GBE_local_lobby, GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 24 cache update for 7044 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string response_7113;
    if (send_join_response) {
        if (!gbe::gc_message::build_dota_practice_lobby_join_response_payload(has_request_job, request_job_id, 0u, response_7113)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7113 join response LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }
    }

    gbe::dota_gc_router::DotaGcOutboundMessage outbound_24{};
    gbe::dota_gc_router::DotaGcOutboundMessage outbound_7113{};
    if (!gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaCacheSubscribed,
            response_24,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_24) ||
        (send_join_response && !gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaPracticeLobbyJoinResponse,
            response_7113,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_7113))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building outbound 7044 responses LobbyID=%llu wrapped=%d", static_cast<unsigned long long>(GBE_local_lobby.lobby_id), wrapped ? 1 : 0);
        return true;
    }

    GBE_RecordDotaLobbyCacheSubscriptionState(response_24, wrapped ? "7044_join_wrapped" : "7044_join_direct");
    push_incoming_now(outbound_24.emsg, outbound_24.payload);
    GBE_LogDotaResponsePacket("7044_join_24", GBE_kDotaCacheSubscribed, wrapped, response_24, outbound_24.payload, GBE_local_lobby.lobby_id, GBE_local_lobby.state, GBE_local_lobby.game_state);
    if (send_join_response) {
        push_incoming_now(outbound_7113.emsg, outbound_7113.payload);
        GBE_LogDotaResponsePacket("7044_join_7113", GBE_kDotaPracticeLobbyJoinResponse, wrapped, response_7113, outbound_7113.payload, GBE_local_lobby.lobby_id, GBE_local_lobby.state, GBE_local_lobby.game_state);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Joined practice lobby via 7044 LobbyID=%llu generic_lobby_id=%llu matched_generic=%u request_job=%llu has_job=%u pass_len=%zu wrapped=%d",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        matched_generic_lobby ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1u : 0u,
        GBE_local_lobby.pass_key.size(),
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaInviteToLobbyRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaInviteToLobbyRequest request{};
    if (!gbe::proto_wire::parse_dota_invite_to_lobby_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 4512 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    uint64 dota_lobby_id = GBE_local_lobby.lobby_id;
    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if ((!GBE_local_lobby.active || dota_lobby_id == 0 || !generic_lobby_id.IsLobby()) && GBE_HasSharedDotaLobbyState()) {
        dota_lobby_id = GBE_GetSharedDotaLobbyIdOrZero();
        generic_lobby_id = CSteamID((uint64)GBE_GetSharedDotaGenericLobbyIdOrZero());
    }

    Steam_Client *steam_client = get_steam_client();
    bool sent_invite = false;
    bool sent_lobby_snapshot = false;
    if (steam_client && steam_client->steam_matchmaking && generic_lobby_id.IsLobby() && request.steam_id != 0) {
        sent_lobby_snapshot = steam_client->steam_matchmaking->SendLobbySnapshotToUserForDotaInvite(generic_lobby_id, CSteamID((uint64)request.steam_id));
        sent_invite = steam_client->steam_matchmaking->InviteUserToLobby(generic_lobby_id, CSteamID((uint64)request.steam_id));
    }

    bool sent_dota_invite = false;
    if (sent_invite && sent_lobby_snapshot && dota_lobby_id != 0)
        sent_dota_invite = true;

    std::string response_4502;
    if (!gbe::gc_message::build_dota_invitation_created_payload(dota_lobby_id, request.steam_id, false, response_4502)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 4502 invite response dota_lobby_id=%llu", static_cast<unsigned long long>(dota_lobby_id));
        return true;
    }

    if (!GBE_PushDotaResponse(GBE_kGCInvitationCreated, response_4502, wrapped, outer_session_field_raw, "4512_invitation_created"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 4512 invite dota_lobby_id=%llu generic_lobby_id=%llu invitee=%llu client_version=%u sent_invite=%u sent_lobby_snapshot=%u sent_dota_invite=%u wrapped=%d",
        static_cast<unsigned long long>(dota_lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(request.steam_id),
        request.has_client_version ? request.client_version : 0u,
        sent_invite ? 1u : 0u,
        sent_lobby_snapshot ? 1u : 0u,
        sent_dota_invite ? 1u : 0u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLobbyInviteResponseRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaLobbyInviteResponseRequest request{};
    if (!gbe::proto_wire::parse_dota_lobby_invite_response_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 4513 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    CSteamID matched_generic_lobby_id = k_steamIDNil;
    GBE_LocalLobby matched_lobby{};
    bool matched_generic_lobby = request.has_lobby_id && request.lobby_id != 0 &&
        GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "4513_invite_accept");

    Steam_Client *steam_client = get_steam_client();
    if (!matched_generic_lobby && steam_client && steam_client->steam_matchmaking && request.has_lobby_id && request.lobby_id != 0) {
        matched_generic_lobby_id = steam_client->steam_matchmaking->FindLobbyByDotaLobbyIdForInvite(
            request.lobby_id,
            GBE_kDotaGenericLobbyMarkerKey,
            GBE_kDotaGenericLobbyMarkerValue,
            GBE_kDotaGenericLobbyDotaLobbyIdKey);
        if (matched_generic_lobby_id.IsLobby()) {
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
            matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "4513_invite_accept_local_find");
        }
    }

    if (request.has_accept && !request.accept) {
        std::string response_remove_2011;
        if (request.has_lobby_id && request.lobby_id != 0 && gbe::gc_message::build_dota_remove_lobby_invite_payload(request.lobby_id, settings->get_local_steam_id().ConvertToUint64(), response_remove_2011)) {
            if (!wrapped || outer_session_field_raw)
                GBE_PushDotaResponse(GBE_kDotaPracticeLobbyDetailsUpdate, response_remove_2011, wrapped, outer_session_field_raw, "4513_decline_remove_2011");
        }

        std::string response_25;
        if (gbe::gc_message::build_dota_so_owner_cache_unsubscribed_payload(4u, settings->get_local_steam_id().ConvertToUint64(), response_25)) {
            if (!wrapped || outer_session_field_raw)
                GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "4513_decline_25");
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Processed declined 4513 invite response lobby_id=%llu matched_generic=%u generic_lobby_id=%llu client_version=%u wrapped=%d",
            static_cast<unsigned long long>(request.lobby_id),
            matched_generic_lobby ? 1u : 0u,
            static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
            request.has_client_version ? request.client_version : 0u,
            wrapped ? 1 : 0);
        return true;
    }

    std::string join_body;
    if (request.has_lobby_id)
        gbe::proto_wire::append_varint_field(join_body, 1u, request.lobby_id);

    if (!GBE_HandleDotaPracticeLobbyJoinRequest(join_body, 0u, false, wrapped, outer_session_field_raw, false))
        return false;

    std::string response_remove_2011;
    if (request.has_lobby_id && request.lobby_id != 0 && gbe::gc_message::build_dota_remove_lobby_invite_payload(request.lobby_id, settings->get_local_steam_id().ConvertToUint64(), response_remove_2011)) {
        if (!wrapped || outer_session_field_raw)
            GBE_PushDotaResponse(GBE_kDotaPracticeLobbyDetailsUpdate, response_remove_2011, wrapped, outer_session_field_raw, "4513_accept_remove_2011");
    }

    std::string response_25;
    if (gbe::gc_message::build_dota_so_owner_cache_unsubscribed_payload(4u, settings->get_local_steam_id().ConvertToUint64(), response_25)) {
        if (!wrapped || outer_session_field_raw)
            GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "4513_accept_25");
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 4513 invite response lobby_id=%llu accept=%u matched_generic=%u generic_lobby_id=%llu client_version=%u wrapped=%d",
        static_cast<unsigned long long>(request.lobby_id),
        (!request.has_accept || request.accept) ? 1u : 0u,
        matched_generic_lobby ? 1u : 0u,
        static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
        request.has_client_version ? request.client_version : 0u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaFriendLobbyInviteMessage(Common_Message *msg)
{
    if (!msg || !msg->has_friend_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (msg->friend_messages().type() != Friend_Messages::LOBBY_INVITE)
        return false;

    CSteamID generic_lobby_id((uint64)msg->friend_messages().lobby_id());
    if (!generic_lobby_id.IsLobby())
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();

    const char *marker = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMarkerKey);
    if (!marker || std::strcmp(marker, GBE_kDotaGenericLobbyMarkerValue) != 0)
        return false;

    const uint64 dota_lobby_id = gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyDotaLobbyIdKey));
    if (dota_lobby_id == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring friend lobby invite without Dota lobby id generic_lobby_id=%llu source=%llu",
            static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
            static_cast<unsigned long long>(msg->source_id())
        );
        return true;
    }

    uint64 owner_steam_id = gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey));
    if (owner_steam_id == 0)
        owner_steam_id = msg->source_id();
    const uint64 inviter_steam_id = msg->source_id() != 0 ? msg->source_id() : owner_steam_id;

    const char *owner_name_value = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey);
    const std::string owner_name = owner_name_value && owner_name_value[0] != '\0'
        ? std::string(owner_name_value)
        : std::string("Lobby Host");

    auto get_invite_member_name = [&](uint64 steam_id) -> std::string {
        CSteamID member_id((uint64)steam_id);
        const char *member_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberNameKey);
        if (member_name && member_name[0] != '\0')
            return std::string(member_name);
        if (steam_id == owner_steam_id || steam_id == inviter_steam_id)
            return owner_name;
        return std::string("Lobby Host");
    };

    std::vector<std::pair<uint64, std::string>> invite_members;
    const std::string inviter_name = get_invite_member_name(inviter_steam_id);
    if (inviter_steam_id != 0)
        invite_members.emplace_back(inviter_steam_id, inviter_name);
    for (const auto &member_id : steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id)) {
        const uint64 member_steam_id = member_id.ConvertToUint64();
        if (member_steam_id == 0 || member_steam_id == inviter_steam_id)
            continue;
        invite_members.emplace_back(member_steam_id, get_invite_member_name(member_steam_id));
    }

    const char *room_name_value = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey);
    const std::string room_name = room_name_value && room_name_value[0] != '\0'
        ? std::string(room_name_value)
        : owner_name;

    std::string invite_24;
    if (!GBE_AdaptDotaLobbyInviteCacheSubscribedPayload(
            dota_lobby_id,
            inviter_steam_id,
            settings->get_local_steam_id().ConvertToUint64(),
            inviter_name,
            invite_members,
            invite_24)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building 2011 from friend lobby invite dota_lobby_id=%llu generic_lobby_id=%llu source=%llu",
            static_cast<unsigned long long>(dota_lobby_id),
            static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
            static_cast<unsigned long long>(msg->source_id())
        );
        return true;
    }

    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, invite_24);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Created 2011 lobby invite from friend invite dota_lobby_id=%llu generic_lobby_id=%llu source=%llu owner=%llu members=%zu room='%s'",
        static_cast<unsigned long long>(dota_lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(msg->source_id()),
        static_cast<unsigned long long>(owner_steam_id),
        invite_members.size(),
        room_name.c_str()
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaNetworkLobbyInviteMessage(Common_Message *msg)
{
    if (!msg || !msg->has_steam_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (msg->steam_messages().type() != Steam_Messages::FRIEND_CHAT)
        return false;

    const std::string &message = msg->steam_messages().message();
    if (!GBE_IsDotaLobbyInviteCacheSubscribedPayload(message))
        return false;

    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, message);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received network 2011 lobby invite source=%llu size=%zu",
        static_cast<unsigned long long>(msg->source_id()),
        message.size()
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaAbandonCurrentGameRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7035 because no local lobby is active");
        return true;
    }

    gbe::dota_lobby_state::DotaAbandonRequestContext request{};
    if (!gbe::dota_lobby_state::build_dota_abandon_request_context(
            GBE_local_lobby,
            wrapped,
            outer_session_field_raw != nullptr,
            is_server,
            request)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7035 because request context could not be built");
        return true;
    }

    const gbe::dota_lobby_state::AbandonDecision d = gbe::dota_lobby_state::compute_abandon_decision(request);

    if (d.arcade_launch_failed_before_connect && GBE_local_lobby.game_start_time != 0u) {
        const uint32 now = static_cast<uint32>(std::time(nullptr));
        if (now <= GBE_local_lobby.game_start_time + 5u) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignoring early arcade direct 7035 during launch grace window LobbyID=%llu state=%u game_state=%u launch_phase=%s start_time=%u now=%u",
                static_cast<unsigned long long>(d.lobby_id),
                d.lobby_state,
                d.lobby_game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                GBE_local_lobby.game_start_time,
                now
            );
            return true;
        }
    }
    if (d.arcade_launch_failed_before_connect) {
        std::string response_25;
        if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(d.lobby_id, response_25)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for arcade launch failed 7035 LobbyID=%llu", static_cast<unsigned long long>(d.lobby_id));
            return true;
        }

        if (d.discard_queued_launch_messages)
            GBE_DiscardQueuedDotaLaunchMessagesForAbandon("7035_arcade_launch_failed_before_connect");
        if (d.set_pending_reset_after_cache_unsubscribed)
            GBE_SetPendingResetAfterCacheUnsubscribed(d.lobby_id);
        if (d.suppress_abandoned_lobby)
            GBE_MarkDotaAbandonedLobbySuppressed(d.lobby_id, "7035_arcade_launch_failed_before_connect");
        push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Treated arcade launch 7035 before connect as failed launch. queued 25 and skipped postgame LobbyID=%llu state=%u game_state=%u launch_phase=%s",
            static_cast<unsigned long long>(d.lobby_id),
            d.lobby_state,
            d.lobby_game_state,
            GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
        );
        return true;
    }
    if (!d.ready_for_abandon_teardown) {
        if (d.queue_cache_unsubscribed) {
            std::string response_25;
            if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(d.lobby_id, response_25)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for current-game 7035 LobbyID=%llu", static_cast<unsigned long long>(d.lobby_id));
                return true;
            }

            if (d.set_pending_reset_after_cache_unsubscribed)
                GBE_SetPendingResetAfterCacheUnsubscribed(d.lobby_id);
            if (d.suppress_abandoned_lobby)
                GBE_MarkDotaAbandonedLobbySuppressed(d.lobby_id, "7035_current_game_disconnect");
            push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);

            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Treated 7035 as current-game disconnect. queued 25 and deferred reset until retrieval LobbyID=%llu state=%u game_state=%u owner_connected=%u",
                static_cast<unsigned long long>(d.lobby_id),
                d.lobby_state,
                d.lobby_game_state,
                GBE_local_lobby.owner_connected ? 1u : 0u
            );
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring early 7035 before launch reaches a current-game stage LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            static_cast<unsigned long long>(d.lobby_id),
            d.lobby_state,
            d.lobby_game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return true;
    }

    if (d.require_wrapped_session) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7035 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    if (d.discard_queued_launch_messages)
        GBE_DiscardQueuedDotaLaunchMessagesForAbandon("7035_ready_for_abandon_teardown");
    if (d.suppress_abandoned_lobby)
        GBE_MarkDotaAbandonedLobbySuppressed(d.lobby_id, "7035_ready_for_abandon_teardown");

    if (d.queue_postgame_teardown && !GBE_QueueDotaPostGameTeardown(
            "7035_abandon_current_game",
            wrapped,
            outer_session_field_raw,
            d.suppress_previous_chat_channel,
            d.push_postgame_cache_unsubscribed,
            d.push_postgame_join))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 7035. sent 25 and postgame 7010 wrapped=%d LobbyID=%llu",
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(d.lobby_id)
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaGameMatchSignOutRequest(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint64 match_id = GBE_local_lobby.match_id;
    const uint32 game_start_time = GBE_local_lobby.game_start_time;
    const uint32 signout_time = static_cast<uint32>(std::time(nullptr));
    const uint32 duration = game_start_time != 0u ? signout_time - game_start_time : 0u;

    std::string response_7005;
    if (!gbe::gc_message::build_dota_game_match_sign_out_response_payload(match_id, duration, signout_time, has_request_job, request_job_id, response_7005)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7005 signout response request_job=%llu has_job=%d", static_cast<unsigned long long>(request_job_id), has_request_job ? 1 : 0);
        return true;
    }

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        if (GBE_local_lobby.game_state < 6u)
            GBE_local_lobby.game_state = 6u;
        if (GBE_local_lobby.state < 2u)
            GBE_local_lobby.state = 2u;
        GBE_PublishSharedDotaLobbyState("7004_signout_post_game");
        GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7004_signout_run_post_game");
    }

    if (!GBE_PushDotaResponse(GBE_kDotaGameMatchSignOutResponse, response_7005, wrapped, outer_session_field_raw, "7004_signout_response"))
        return true;

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        GBE_QueueDotaPostGameTeardown("7004_signout_postgame", wrapped, outer_session_field_raw, false, false, false);
        GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7004_signout_postgame_state");

        std::string response_25;
        if (gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
            GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "25_after_7004");
            GBE_SetPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(lobby_id);
        } else {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 after 7004 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 7004 signout. replied 7005 and queued postgame teardown wrapped=%d LobbyID=%llu match_id=%llu request_job=%llu has_job=%d",
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(match_id),
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLeaveRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7040 because no local lobby is active");
        return true;
    }

    if (GBE_local_lobby.state == 2u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring stale 7040 during active runtime lobby LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu wrapped=%d",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            wrapped ? 1 : 0
        );
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    uint64 fallback_generic_lobby_id = 0ull;
    if (GBE_local_lobby.generic_lobby_id == 0ull) {
        CSteamID matched_generic_lobby_id = k_steamIDNil;
        GBE_LocalLobby matched_lobby{};
        bool matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(lobby_id, matched_generic_lobby_id, &matched_lobby, "7040_leave");
        Steam_Client *steam_client = get_steam_client();
        if (!matched_generic_lobby && steam_client && steam_client->steam_matchmaking) {
            matched_generic_lobby_id = steam_client->steam_matchmaking->FindLobbyByDotaLobbyIdForInvite(
                lobby_id,
                GBE_kDotaGenericLobbyMarkerKey,
                GBE_kDotaGenericLobbyMarkerValue,
                GBE_kDotaGenericLobbyDotaLobbyIdKey);
            matched_generic_lobby = matched_generic_lobby_id.IsLobby();
        }
        if (matched_generic_lobby && matched_generic_lobby_id.IsLobby()) {
            fallback_generic_lobby_id = matched_generic_lobby_id.ConvertToUint64();
            GBE_local_lobby.generic_lobby_id = fallback_generic_lobby_id;
            GBE_SyncSettingsLobbyFromGenericLobby("7040_leave_local_find");
        }
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7040 leave fallback generic lookup LobbyID=%llu generic_lobby_id=%llu matched=%u members=%zu",
            static_cast<unsigned long long>(lobby_id),
            static_cast<unsigned long long>(fallback_generic_lobby_id),
            matched_generic_lobby ? 1u : 0u,
            matched_lobby.members.size()
        );
    }
    std::string response_25;
    if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    GBE_MarkDotaAbandonedLobbySuppressed(lobby_id, "7040_leave");

    if (!GBE_PushDotaCacheUnsubscribedResponse(response_25, wrapped, outer_session_field_raw, "7040_leave_25"))
        return true;
    ResetGCMemory("7040_leave", true, false);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby leave requested. sent 25 and left generic lobby LobbyID=%llu fallback_generic_lobby_id=%llu wrapped=%d",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(fallback_generic_lobby_id),
        wrapped ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLaunchRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7041 because no local lobby is active");
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();

    const uint32 launch_ip = network ? network->getOwnIP() : 0u;
    const gbe::dota_lobby_state::LaunchInitPlan launch_plan = gbe::dota_lobby_state::compose_launch_init_plan(
        GBE_local_lobby,
        GBE_GenerateDotaMatchId(),
        gbe::dota_custom_game::derive_practice_lobby_ip_server_id(launch_ip),
        gbe::proto_wire::format_dota_practice_lobby_connect_from_ip(launch_ip),
        static_cast<uint32>(std::time(nullptr)),
        GBE_kDotaLaunchPhaseRequested);
    GBE_local_lobby = launch_plan.lobby;
    GBE_PublishSharedDotaLobbyState("7041_launch_init");

    if (gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
        const gbe::dota_lobby_state::LaunchPresenceEvent presence_event = gbe::dota_lobby_state::compose_launch_serversetup_presence_event("7041_custom_game_launch_init");
        if (presence_event.update) {
            GBE_UpdateDotaPracticeLobbyLaunchRichPresence(presence_event.status.c_str(), presence_event.lobby_state.c_str(), presence_event.include_party);
            GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(presence_event.status.c_str(), presence_event.lobby_state.c_str(), presence_event.include_party, presence_event.include_lobby, presence_event.persona_reason.c_str());
        }

        if (GBE_SendDotaCustomGameLaunchSetupFlow(wrapped, outer_session_field_raw, has_request_job, request_job_id)) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Deferred custom game RUN until 8052 after 7041 LobbyID=%llu match_id=%llu server_id=%llu custom_id=%llu custom_map=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
                GBE_local_lobby.custom_game.map_name.c_str()
            );
            return true;
        }
    }

    const gbe::dota_lobby_state::PracticeLobbyLaunchEventPlan event_plan = gbe::dota_lobby_state::compose_practice_lobby_launch_event_plan(GBE_kDotaPracticeLobbyDetailsUpdate);
    std::string stage1_message;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(GBE_local_lobby, GBE_local_lobby.owner_name, stage1_message, true)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building initial 26 after 7041 LobbyID=%llu match_id=%llu server_id=%llu connect=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            GBE_local_lobby.connect.c_str()
        );
        return true;
    }

    std::string wrapped_stage1_message;
    if (!event_plan.initial_details.send || !GBE_PushDotaResponse(event_plan.initial_details.emsg, stage1_message, wrapped, outer_session_field_raw, event_plan.initial_details.reason.c_str(), event_plan.initial_details.apply_lobby_state, event_plan.initial_details.lobby_state, event_plan.initial_details.lobby_game_state, &wrapped_stage1_message))
        return true;
    if (wrapped)
        stage1_message.swap(wrapped_stage1_message);

    if (event_plan.steam_auth_ack.queue)
        GBE_MaybeQueueDotaPracticeLobbySteamAuthAck(event_plan.steam_auth_ack.reason.c_str(), has_request_job ? request_job_id : 0ull);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent initial 26 after 7041 path=%s has_request_job=%d request_job=%llu LobbyID=%llu match_id=%llu server_id=%llu game_start=%u connect=%s size=%zu body_prefix=%s",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_local_lobby.game_start_time,
        GBE_local_lobby.connect.c_str(),
        stage1_message.size(),
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(stage1_message.data()), stage1_message.size(), 32).c_str()
    );

    if (event_plan.presence.update) {
        GBE_UpdateDotaPracticeLobbyLaunchRichPresence(event_plan.presence.status.c_str(), event_plan.presence.lobby_state.c_str(), event_plan.presence.include_party);
        GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(event_plan.presence.status.c_str(), event_plan.presence.lobby_state.c_str(), event_plan.presence.include_party, event_plan.presence.include_lobby, event_plan.presence.persona_reason.c_str());
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Deferring remaining 7041 launch follow-ups until server_id sync LobbyID=%llu match_id=%llu server_id=%llu",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id)
    );

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbySetDetailsRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7046 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyDetailsRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_set_details_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7046 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    if (request.has_lobby_id && request.lobby_id != GBE_local_lobby.lobby_id) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7046 LobbyID mismatch request=%llu local=%llu, keeping local state",
            static_cast<unsigned long long>(request.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
    }

    if (request.has_room_name)
        GBE_local_lobby.room_name = request.room_name;
    if (request.has_server_region)
        GBE_local_lobby.server_region = request.server_region;
    if (request.has_lan)
        GBE_local_lobby.lan = request.lan;
    if (request.has_lan_host_ping_location)
        GBE_local_lobby.lan_host_ping_location = request.lan_host_ping_location;
    if (request.has_game_mode)
        GBE_local_lobby.game_mode = request.game_mode;
    if (request.has_bot_difficulty_radiant)
        GBE_local_lobby.bot_difficulty_radiant = request.bot_difficulty_radiant;
    if (request.has_allow_cheats)
        GBE_local_lobby.allow_cheats = request.allow_cheats;
    if (request.has_fill_with_bots)
        GBE_local_lobby.fill_with_bots = request.fill_with_bots;
    if (request.has_allow_spectating)
        GBE_local_lobby.allow_spectating = request.allow_spectating;
    if (request.has_pass_key)
        GBE_local_lobby.pass_key = request.pass_key;
    if (request.has_visibility)
        GBE_local_lobby.visibility = request.visibility;
    if (request.has_bot_difficulty_dire)
        GBE_local_lobby.bot_difficulty_dire = request.bot_difficulty_dire;
    if (request.has_bot_radiant)
        GBE_local_lobby.bot_radiant = request.bot_radiant;
    if (request.has_bot_dire)
        GBE_local_lobby.bot_dire = request.bot_dire;
    GBE_ApplyDotaCustomGameDetailsRequest(request, GBE_local_lobby.custom_game);
    GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, GBE_local_lobby.custom_game);
    GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby);

    if (!GBE_PublishDotaPracticeLobbySetDetailsUpdate(wrapped, outer_session_field_raw))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Room details updated. mode=%u server_region=%u lan=%u lan_ping=%s cheats=%u bots=%u spectating=%u visibility=%u bot_diff_r=%u bot_diff_d=%u bot_radiant=%llu bot_dire=%llu name=%s password_len=%zu",
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan ? 1u : 0u,
        GBE_local_lobby.lan_host_ping_location.c_str(),
        GBE_local_lobby.allow_cheats ? 1u : 0u,
        GBE_local_lobby.fill_with_bots ? 1u : 0u,
        GBE_local_lobby.allow_spectating ? 1u : 0u,
        GBE_local_lobby.visibility,
        GBE_local_lobby.bot_difficulty_radiant,
        GBE_local_lobby.bot_difficulty_dire,
        static_cast<unsigned long long>(GBE_local_lobby.bot_radiant),
        static_cast<unsigned long long>(GBE_local_lobby.bot_dire),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.pass_key.size()
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbySetTeamSlotRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7047 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbySetTeamSlotRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_set_team_slot_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7047 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    const bool local_is_owner = local_steam_id != 0ull && local_steam_id == GBE_local_lobby.owner_steam_id;
    if (local_is_owner) {
        if (request.has_team)
            GBE_local_lobby.owner_team = request.team;
        if (request.has_slot)
            GBE_local_lobby.owner_slot = request.slot;
    }
    gbe::dota_lobby_flow::apply_lobby_member_team_slot_update(
        GBE_local_lobby.members,
        local_steam_id,
        settings->get_local_steam_id().GetAccountID(),
        request.has_team,
        request.team,
        request.has_slot,
        request.slot,
        GBE_kDotaTeamPlayerPool,
        GBE_local_lobby.state == 3u);
    if (request.has_bot_difficulty) {
        const uint32 bot_team = request.has_team ? request.team : GBE_local_lobby.owner_team;
        if (gbe::proto_wire::dota_is_dire_team(bot_team))
            GBE_local_lobby.bot_difficulty_dire = request.bot_difficulty;
        else
            GBE_local_lobby.bot_difficulty_radiant = request.bot_difficulty;
    }
    GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby);
    GBE_PublishDotaPracticeLobbyLocalMemberData("7047_set_team_slot");
    GBE_PublishSharedDotaLobbyState("7047_set_team_slot");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7047"))
        return true;

    if (has_request_job) {
        std::string response_7055;
        if (!gbe::gc_message::build_dota_practice_lobby_response_payload(request_job_id, true, response_7055)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for 7047 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        std::string wrapped_7055;
        if (!GBE_PushDotaPracticeLobbyResponse(response_7055, wrapped, outer_session_field_raw, "7047_7055", &wrapped_7055))
            return true;

        if (wrapped) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Sent wrapped 7055 ack for 7047 LobbyID=%llu request_job=%llu size=%zu body_prefix=%s packet_prefix=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(request_job_id),
                wrapped_7055.size(),
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str(),
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(wrapped_7055.data()), wrapped_7055.size(), 32).c_str()
            );
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Sent direct 7055 ack for 7047 LobbyID=%llu request_job=%llu size=%zu body_prefix=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(request_job_id),
                response_7055.size(),
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str()
            );
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Team slot updated. local_team=%u local_slot=%u owner_team=%u owner_slot=%u bot_diff_req=%u has_bot_diff=%d",
        request.has_team ? request.team : 0u,
        request.has_slot ? request.slot : 0u,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        request.bot_difficulty,
        request.has_bot_difficulty ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyKickRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because no local lobby is active");
        return true;
    }

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    if (local_steam_id != GBE_local_lobby.owner_steam_id) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because local user is not Dota lobby owner LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_DotaPracticeLobbyKickRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_kick_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7081 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    if (request.account_id == 0u || request.account_id == settings->get_local_steam_id().GetAccountID()) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 invalid target account_id=%u LobbyID=%llu", request.account_id, static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    const uint64 kicked_steam_id = gbe::dota_lobby_flow::find_lobby_member_steam_id_by_account_id(GBE_local_lobby.members, request.account_id);

    if (kicked_steam_id == 0ull) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because target account_id=%u is not in Dota lobby LobbyID=%llu", request.account_id, static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because matchmaking is unavailable LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_LocalLobby before_lobby = GBE_local_lobby;
    gbe::dota_lobby_flow::clear_lobby_member_by_account_id(before_lobby.members, request.account_id);

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    const bool kicked = steam_client->steam_matchmaking->KickLobbyMemberForDota(generic_lobby_id, CSteamID((uint64)kicked_steam_id));
    if (!kicked) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed kicking generic lobby member for 7081 LobbyID=%llu generic_lobby_id=%llu target_account=%u target_steam=%llu",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
            request.account_id,
            static_cast<unsigned long long>(kicked_steam_id)
        );
        return true;
    }

    GBE_local_lobby = before_lobby;
    GBE_PublishSharedDotaLobbyState("7081_kick_member");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7081"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Kicked practice lobby member target_account=%u target_steam=%llu LobbyID=%llu generic_lobby_id=%llu wrapped=%d",
        request.account_id,
        static_cast<unsigned long long>(kicked_steam_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        wrapped ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaDestroyLobbyRequest(uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 8246 because no local lobby is active");
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    std::string response_25;
    if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    std::string response_8247;
    if (has_request_job) {
        if (!gbe::gc_message::build_dota_destroy_lobby_response_payload(request_job_id, response_8247)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 8247 payload for LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }
    }

    gbe::dota_gc_router::DotaGcOutboundMessage outbound_25{};
    gbe::dota_gc_router::DotaGcOutboundMessage outbound_8247{};
    if (!gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaCacheUnsubscribed,
            response_25,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_25) ||
        (has_request_job && !gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaDestroyLobbyResponse,
            response_8247,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_8247))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building outbound 8246 responses LobbyID=%llu wrapped=%d", static_cast<unsigned long long>(lobby_id), wrapped ? 1 : 0);
        return true;
    }

    ResetGCMemory("8246_destroy", true, true);
    push_incoming_now(outbound_25.emsg, outbound_25.payload);
    GBE_LogDotaResponsePacket("8246_destroy_25", GBE_kDotaCacheUnsubscribed, wrapped, response_25, outbound_25.payload, lobby_id, 0u, 0u);
    if (has_request_job) {
        push_incoming_now(outbound_8247.emsg, outbound_8247.payload);
        GBE_LogDotaResponsePacket("8246_destroy_8247", GBE_kDotaDestroyLobbyResponse, wrapped, response_8247, outbound_8247.payload, lobby_id, 0u, 0u);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby destroyed. unsubscribed LobbyID=%llu wrapped=%d request_job=%llu has_job=%d",
        static_cast<unsigned long long>(lobby_id),
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1 : 0
    );
    return true;
}
