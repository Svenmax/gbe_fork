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
#include "gbe_dota_gc_diagnostics.h"
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

        GBE_ExecuteDotaLifecycleActions(gbe::dota_lobby_flow::abandon_cache_unsubscribed_action_list(
            d,
            response_25,
            "7035_arcade_launch_failed_before_connect"));

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

            GBE_ExecuteDotaLifecycleActions(gbe::dota_lobby_flow::abandon_cache_unsubscribed_action_list(
                d,
                response_25,
                "7035_current_game_disconnect"));

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

    gbe::dota_lifecycle_state_machine::MachineState machine_state{};
    machine_state.generation = GBE_CurrentDotaLobbyGeneration();
    const auto teardown = gbe::dota_lifecycle_state_machine::transition_teardown(
        machine_state,
        { { gbe::dota_lifecycle_state_machine::EventKind::Abandon,
            gbe::dota_lifecycle_state_machine::transport_source(wrapped),
            GBE_kDotaAbandonCurrentGame,
            machine_state.generation },
          gbe::dota_lifecycle_state_machine::TeardownStage::Initiate,
          true,
          GBE_local_lobby.abandon_postgame_active });
    if (!teardown.accepted() || !teardown.effects.contains(
            gbe::dota_lifecycle_state_machine::EffectKind::TeardownAbandonInitiateRequested))
        return true;

    GBE_ExecuteDotaLifecycleActions(gbe::dota_lobby_flow::abandon_initiate_preflight_action_list(
        d,
        "7035_ready_for_abandon_teardown"));

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
        gbe::dota_lifecycle::ExecutionOptions options;
        options.wrapped = wrapped;
        options.outer_session_field_raw = outer_session_field_raw;
        if (!GBE_ExecuteDotaLifecycleActions(
                gbe::dota_lobby_flow::signout_postgame_action_list(
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state),
                options).succeeded)
            return true;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaGameMatchSignOutResponse, response_7005, wrapped, outer_session_field_raw, "7004_signout_response"))
        return true;

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        gbe::dota_lifecycle_state_machine::MachineState machine_state{};
        machine_state.generation = GBE_CurrentDotaLobbyGeneration();
        const auto teardown = gbe::dota_lifecycle_state_machine::transition_teardown(
            machine_state,
            { { gbe::dota_lifecycle_state_machine::EventKind::PostGame,
                gbe::dota_lifecycle_state_machine::transport_source(wrapped),
                GBE_kDotaGameMatchSignOut,
                machine_state.generation },
              gbe::dota_lifecycle_state_machine::TeardownStage::Initiate,
              true,
              GBE_local_lobby.abandon_postgame_active });
        if (!teardown.accepted() || !teardown.effects.contains(
                gbe::dota_lifecycle_state_machine::EffectKind::TeardownPostGameInitiateRequested))
            return true;
        GBE_QueueDotaPostGameTeardown("7004_signout_postgame", wrapped, outer_session_field_raw, false, false, false);

        std::string response_25;
        if (gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
            gbe::dota_lifecycle::ExecutionOptions options;
            options.wrapped = wrapped;
            options.outer_session_field_raw = outer_session_field_raw;
            options.push_route = gbe::dota_lifecycle::PushRoute::DotaResponse;
            GBE_ExecuteDotaLifecycleActions(
                gbe::dota_lobby_flow::normal_signout_postgame_followup_action_list(lobby_id, response_25),
                options);
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
    gbe::dota_lifecycle_state_machine::MachineState machine_state{};
    machine_state.generation = GBE_CurrentDotaLobbyGeneration();
    const auto teardown = gbe::dota_lifecycle_state_machine::transition_teardown(
        machine_state,
        { { gbe::dota_lifecycle_state_machine::EventKind::Leave,
            gbe::dota_lifecycle_state_machine::transport_source(wrapped),
            GBE_kDotaPracticeLobbyLeave,
            machine_state.generation },
          gbe::dota_lifecycle_state_machine::TeardownStage::Initiate,
          true,
          GBE_local_lobby.pending_leave_after_7040 });
    if (!teardown.accepted() || !teardown.effects.contains(
            gbe::dota_lifecycle_state_machine::EffectKind::TeardownLeaveInitiateRequested))
        return true;
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

    gbe::dota_lifecycle::ExecutionOptions leave_options;
    leave_options.wrapped = wrapped;
    leave_options.outer_session_field_raw = outer_session_field_raw;
    leave_options.push_route = gbe::dota_lifecycle::PushRoute::CacheUnsubscribedResponse;
    leave_options.push_reason_override = "7040_leave_25";
    leave_options.abort_on_push_failure = true;
    if (!GBE_ExecuteDotaLifecycleActions(
            gbe::dota_lobby_flow::leave_lobby_cache_unsubscribed_action_list(
                lobby_id,
                response_25,
                "7040_leave"),
            leave_options).succeeded)
        return true;

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

    const GBE_DotaActionList launch_init_actions = gbe::dota_lobby_flow::launch_init_action_list();
    if (launch_init_actions.empty() ||
        launch_init_actions.front().type != GBE_DotaActionType::LaunchPeripheralReset)
        return true;
    if (!GBE_ExecuteDotaLifecycleActions(
            { launch_init_actions.front() }).succeeded)
        return true;

    const uint32 launch_ip = network ? network->getOwnIP() : 0u;
    const gbe::dota_lobby_state::LaunchInitPlan launch_plan = gbe::dota_lobby_state::compose_launch_init_plan(
        GBE_local_lobby,
        GBE_GenerateDotaMatchId(),
        gbe::dota_custom_game::derive_practice_lobby_ip_server_id(launch_ip),
        gbe::proto_wire::format_dota_practice_lobby_connect_from_ip(launch_ip),
        static_cast<uint32>(std::time(nullptr)),
        GBE_kDotaLaunchPhaseRequested);
    GBE_local_lobby = launch_plan.lobby;
    if (launch_init_actions.size() < 2u ||
        launch_init_actions[1].type != GBE_DotaActionType::SharedLobbyPublish ||
        !GBE_ExecuteDotaLifecycleActions({ launch_init_actions[1] }).succeeded)
        return true;

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

    gbe::dota_lifecycle_state_machine::MachineState machine_state{};
    machine_state.generation = GBE_CurrentDotaLobbyGeneration();
    const auto clear_boundary = gbe::dota_lifecycle_state_machine::transition_runtime_clear_boundary(
        machine_state,
        { gbe::dota_lifecycle_state_machine::EventKind::Leave,
          gbe::dota_lifecycle_state_machine::transport_source(wrapped),
          GBE_kDotaDestroyLobbyRequest,
          machine_state.generation });
    if (!clear_boundary.accepted() || !clear_boundary.effects.contains(
            gbe::dota_lifecycle_state_machine::EffectKind::GenerationAdvanced))
        return true;
    if (!GBE_ExecuteDotaLifecycleActions(
            gbe::dota_lobby_flow::destroy_lobby_reset_action_list()).succeeded)
        return true;
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
