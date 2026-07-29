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

// Split from gbe_dota_lobby_handlers.cpp (stage D.10.1). Behavior unchanged.

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_gc_diagnostics.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lifecycle_state_machine.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_lobby_handler_helpers.h"
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
//   4. compose_join_lobby_merge_plan (pure) -> apply_join_lobby_merge_plan
//   5. If matched_generic_lobby: JoinLobby + SyncSettingsLobby [coordinator]
//   6. pass_key is carried in the join merge plan when present
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
//      c. Set GBE_pending_reset_after_cache_unsubscribed_slot [coordinator mutation]
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
//   5. clear_local_lobby [state helper]
//   6. GBE_LeaveGenericLobby [coordinator]
//   7. If has_request_job: build 7055 + PushDotaResponse(7055) [coordinator]
//   Invariant: 25 precedes lobby clear precedes generic lobby leave.
// ============================================================================


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyCreateRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaPracticeLobbyCreateRequest pre_reset_request{};
    GBE_DotaCustomGameDetails pre_reset_custom_game{};
    const bool parsed_pre_reset_request = gbe::proto_wire::parse_dota_practice_lobby_create_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), pre_reset_request);
    if (parsed_pre_reset_request && pre_reset_request.has_lobby_details) {
        GBE_ApplyDotaCustomGameDetailsRequest(pre_reset_request.lobby_details, pre_reset_custom_game);
        GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, pre_reset_custom_game);
    }

    GBE_DotaPracticeLobbyCreateRequest request{};
    const bool parsed_create_request = gbe::proto_wire::parse_dota_practice_lobby_create_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request);

    gbe::dota_lobby_flow::CreateLobbyContext create_context{};
    create_context.previous_lobby = gbe::dota_lobby_state::LocalLobbyOwner(GBE_local_lobby).snapshot();
    create_context.pre_reset_custom_game = pre_reset_custom_game;
    create_context.request = request;
    create_context.parsed_request = parsed_create_request;
    create_context.new_lobby_id = GBE_GenerateDotaLobbyId();
    create_context.owner_steam_id = settings->get_local_steam_id().ConvertToUint64();
    create_context.owner_account_id = settings->get_local_steam_id().GetAccountID();
    create_context.owner_name = std::string(settings->get_local_name());
    create_context.owner_team = GBE_kDotaTeamGoodGuys;
    create_context.owner_slot = 1u;

    const gbe::dota_lobby_state::CreateLobbyResetPlan reset_plan = gbe::dota_lobby_flow::create_lobby_reset_plan_from_context(create_context);
    gbe::dota_lifecycle_state_machine::MachineState machine_state{};
    machine_state.generation = GBE_CurrentDotaLobbyGeneration();
    const auto generation_boundary = gbe::dota_lifecycle_state_machine::transition_generation_boundary(
        machine_state,
        { gbe::dota_lifecycle_state_machine::EventKind::Create,
          gbe::dota_lifecycle_state_machine::transport_source(wrapped),
          GBE_kDotaPracticeLobbyCreate,
          machine_state.generation });
    if (!generation_boundary.accepted() || !generation_boundary.effects.contains(
            gbe::dota_lifecycle_state_machine::EffectKind::GenerationAdvanced))
        return true;
    if (GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Create, "7038_create") == GBE_DotaGenerationAdvanceResult::Exhausted)
        return true;
    const GBE_DotaActionList create_actions = gbe::dota_lobby_flow::create_lobby_action_list(
        gbe::dota_lobby_flow::create_lobby_action_plan_from_reset_plan(reset_plan),
        wrapped);
    std::size_t create_action_index = 0u;

    if (create_action_index < create_actions.size() &&
            create_actions[create_action_index].type == GBE_DotaActionType::GcMemoryReset) {
        ResetGCMemory(
            create_actions[create_action_index].reason.c_str(),
            create_actions[create_action_index].leave_generic_lobby,
            create_actions[create_action_index].clear_queued_messages,
            gbe::dota_lobby_generation::Boundary::Create,
            true);
        ++create_action_index;
    }

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

    const gbe::dota_lobby_state::CreateLobbyPlan create_plan = gbe::dota_lobby_flow::create_lobby_state_plan_from_context(create_context);
    const gbe::dota_lobby_state::CreateLobbyStateApplyPlan state_apply_plan = gbe::dota_lobby_state::compose_create_lobby_state_apply_plan(
        create_plan,
        create_context.parsed_request && create_context.request.has_lobby_details);
    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    local_lobby.apply("7038_create", [this, &state_apply_plan](GBE_LocalLobby &lobby) {
        gbe::dota_lobby_state::apply_create_lobby_state_plan(lobby, state_apply_plan);
        gbe::dota_lobby_state::apply_lobby_generation(lobby, GBE_CurrentDotaLobbyGeneration());
        if (state_apply_plan.normalize_custom_game_details)
            GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, lobby.custom_game);
        if (state_apply_plan.normalize_arcade_member_slots)
            GBE_NormalizeDotaArcadeLobbyMemberSlots(lobby);
    });
    if (state_apply_plan.clear_reconnect_context)
        GBE_ClearRecentDotaReconnectContext();
    if (state_apply_plan.set_reconnect_eligible)
        GBE_SetDotaReconnectEligible(true);
    if (state_apply_plan.log_arcade_isolation) {
        const GBE_LocalLobby &local_lobby_snapshot = local_lobby.snapshot();
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Isolated arcade lobby from prior practice runtime lobby_id=%llu custom_game_id=%llu",
            static_cast<unsigned long long>(local_lobby_snapshot.lobby_id),
            static_cast<unsigned long long>(local_lobby_snapshot.custom_game.game_id)
        );
    }

    Steam_Client *steam_client = get_steam_client();

    for (; create_action_index < create_actions.size(); ++create_action_index) {
        const GBE_DotaAction &action = create_actions[create_action_index];
        if (action.type == GBE_DotaActionType::LobbyCacheSubscriptionRecord)
            break;
        switch (action.type) {
            case GBE_DotaActionType::GenericLobbyCreate:
                if (steam_client && steam_client->steam_matchmaking) {
                    CSteamID generic_lobby_id = steam_client->steam_matchmaking->CreateLobbyImmediate(k_ELobbyTypeInvisible, 10);
                    if (generic_lobby_id.IsLobby()) {
                        const uint64 generic_lobby_id_value = generic_lobby_id.ConvertToUint64();
                        local_lobby.apply("7038_generic_lobby_create", [generic_lobby_id_value](GBE_LocalLobby &lobby) {
                            gbe::dota_lobby_state::apply_lobby_generic_lobby_id(lobby, generic_lobby_id_value);
                        });
                    }
                }
                break;
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

    const GBE_LocalLobby &created_lobby_snapshot = local_lobby.snapshot();
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] State creating path=%s has_job=%u request_job=%llu NewLobbyID=%llu GenericLobbyID=%llu room=%s server_region=%u lan=%u lan_ping=%s mode=%u pass_len=%zu custom_id=%llu custom_mode=%s custom_map=%s custom_min=%u custom_max=%u",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(created_lobby_snapshot.lobby_id),
        static_cast<unsigned long long>(created_lobby_snapshot.generic_lobby_id),
        created_lobby_snapshot.room_name.c_str(),
        created_lobby_snapshot.server_region,
        created_lobby_snapshot.lan ? 1u : 0u,
        created_lobby_snapshot.lan_host_ping_location.c_str(),
        created_lobby_snapshot.game_mode,
        created_lobby_snapshot.pass_key.size(),
        static_cast<unsigned long long>(created_lobby_snapshot.custom_game.game_id),
        created_lobby_snapshot.custom_game.mode.c_str(),
        created_lobby_snapshot.custom_game.map_name.c_str(),
        created_lobby_snapshot.custom_game.min_players,
        created_lobby_snapshot.custom_game.max_players
    );

    std::string response_24;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    if (!GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building template 24 cache update for LobbyID=%llu", static_cast<unsigned long long>(created_lobby_snapshot.lobby_id));
        return true;
    }

    std::string response_7055;
    if (!gbe::gc_message::build_dota_practice_lobby_response_payload(request_job_id, has_request_job, response_7055)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for LobbyID=%llu", static_cast<unsigned long long>(created_lobby_snapshot.lobby_id));
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
            static_cast<unsigned long long>(created_lobby_snapshot.lobby_id),
            wrapped_24.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_24.data()), response_24.size(), 32).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(wrapped_24.data()), wrapped_24.size(), 32).c_str()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 24 cache update with NewLobbyID=%llu size=%zu body_prefix=%s",
            static_cast<unsigned long long>(created_lobby_snapshot.lobby_id),
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
            static_cast<unsigned long long>(created_lobby_snapshot.lobby_id),
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
            static_cast<unsigned long long>(created_lobby_snapshot.lobby_id),
            has_request_job ? 1u : 0u,
            static_cast<unsigned long long>(request_job_id),
            response_7055.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str()
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Skipping initial 26 details update for 7038 to match official create flow LobbyID=%llu",
        static_cast<unsigned long long>(created_lobby_snapshot.lobby_id)
    );

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbySetDetailsRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    const GBE_LocalLobby &initial_lobby_snapshot = local_lobby.snapshot();
    if (!initial_lobby_snapshot.active || initial_lobby_snapshot.lobby_id == 0) {
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

    if (request.has_lobby_id && request.lobby_id != initial_lobby_snapshot.lobby_id) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7046 LobbyID mismatch request=%llu local=%llu, keeping local state",
            static_cast<unsigned long long>(request.lobby_id),
            static_cast<unsigned long long>(initial_lobby_snapshot.lobby_id)
        );
    }

    local_lobby.apply("7046_details_update", [this, &request](GBE_LocalLobby &lobby) {
        gbe::dota_lobby_state::apply_lobby_details_update(lobby, request);
        GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, lobby.custom_game);
        GBE_NormalizeDotaArcadeLobbyMemberSlots(lobby);
    });

    if (!GBE_PublishDotaPracticeLobbySetDetailsUpdate(wrapped, outer_session_field_raw))
        return true;

    const GBE_LocalLobby &updated_lobby_snapshot = local_lobby.snapshot();
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Room details updated. mode=%u server_region=%u lan=%u lan_ping=%s cheats=%u bots=%u spectating=%u visibility=%u bot_diff_r=%u bot_diff_d=%u bot_radiant=%llu bot_dire=%llu name=%s password_len=%zu",
        updated_lobby_snapshot.game_mode,
        updated_lobby_snapshot.server_region,
        updated_lobby_snapshot.lan ? 1u : 0u,
        updated_lobby_snapshot.lan_host_ping_location.c_str(),
        updated_lobby_snapshot.allow_cheats ? 1u : 0u,
        updated_lobby_snapshot.fill_with_bots ? 1u : 0u,
        updated_lobby_snapshot.allow_spectating ? 1u : 0u,
        updated_lobby_snapshot.visibility,
        updated_lobby_snapshot.bot_difficulty_radiant,
        updated_lobby_snapshot.bot_difficulty_dire,
        static_cast<unsigned long long>(updated_lobby_snapshot.bot_radiant),
        static_cast<unsigned long long>(updated_lobby_snapshot.bot_dire),
        updated_lobby_snapshot.room_name.c_str(),
        updated_lobby_snapshot.pass_key.size()
    );
    return true;
}
