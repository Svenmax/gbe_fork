/* Direct and wrapped custom-game lifecycle request handler. */

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_game_lifecycle.h"
#include "gbe_dota_gc_diagnostics.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_lifecycle_state_machine.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"

bool Steam_Game_Coordinator::GBE_HandleDotaCustomGameLifecycleRequest(const gbe::dota_gc_router::DotaGcRequestContext &context)
{
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received %s custom-game lifecycle emsg=%u has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
        context.wrapped ? "wrapped" : "direct",
        context.inner_emsg,
        context.has_request_job ? 1 : 0,
        static_cast<unsigned long long>(context.request_job_id),
        context.outer_session_field_raw.size(),
        gbe::proto_wire::format_hex_prefix(
            reinterpret_cast<const std::uint8_t *>(context.body.data()),
            context.body.size(),
            48).c_str());

    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    const GBE_LocalLobby &local_lobby_snapshot = local_lobby.snapshot();
    if (!local_lobby_snapshot.active || local_lobby_snapshot.lobby_id == 0 || !gbe::dota_custom_game::has_custom_game_details(local_lobby_snapshot.custom_game))
        return true;

    const uint8 *body = reinterpret_cast<const uint8 *>(context.body.data());
    const size_t body_size = context.body.size();
    const gbe::dota_lifecycle_state_machine::EventMapping event_mapping =
        gbe::dota_lifecycle_state_machine::event_from_message(context.inner_emsg, context.wrapped);
    if (!event_mapping.mapped)
        return true;

    gbe::dota_lifecycle_state_machine::CustomGameRequestState request_state{};
    request_state.machine.generation = GBE_CurrentDotaLobbyGeneration();
    request_state.lobby_state = local_lobby_snapshot.state;
    request_state.game_state = local_lobby_snapshot.game_state;
    request_state.launch_phase = local_lobby_snapshot.launch_phase;
    request_state.has_custom_game = true;
    request_state.has_launch_server_setup = gbe::dota_lobby_state::has_launch_server_setup_sync(local_lobby_snapshot);

    auto execute_decision = [&](const gbe::dota_custom_game_lifecycle::LifecycleDecision &decision) {
        if (!decision.accepted)
            return;
        gbe::dota_custom_game_lifecycle::ExecutionContext execution{};
        execution.transition = decision.transition;
        execution.wrapped = context.wrapped;
        execution.outer_session_field_raw = context.wrapped ? &context.outer_session_field_raw : nullptr;
        execution.trigger_emsg = context.inner_emsg;
        execution.source_job = context.request_job_id;
        execution.runtime_update_note = decision.runtime_update_note;
        execution.update_local_member_runtime = decision.update_local_member_runtime;
        execution.publish_local_member_data = decision.publish_local_member_data;
        GBE_ExecuteDotaCustomGameLifecycleTransition(execution);
    };

    if (context.inner_emsg == 7070u) {
        const auto request = gbe::proto_wire::parse_dota7070_ready_up_request(body, body_size);
        std::string response_7170;
        if (gbe::gc_message::build_dota_ready_up_status_payload(context.has_request_job, context.request_job_id, local_lobby_snapshot.lobby_id, 0u, request.ready_state != 0u ? request.ready_state : 1u, response_7170)) {
            if (context.wrapped)
                GBE_PushDotaResponse(7170u, response_7170, true, &context.outer_session_field_raw, "7070_ready_up_status");
            else
                push_incoming_now(7170u | GBE_kProtoMask, response_7170);
        }

        gbe::dota_lifecycle_state_machine::CustomGameRequest machine_request{};
        machine_request.event = event_mapping.event;
        machine_request.ready_state = request.ready_state;
        execute_decision(gbe::dota_custom_game_lifecycle::decide_ready_up(
            request_state,
            machine_request,
            local_lobby_snapshot,
            GBE_kDotaLaunchPhaseSetupSynced,
            GBE_kDotaLaunchPhaseRunQueued,
            context.wrapped
                ? "7070_wrapped_custom_game_ready_up_run_ack"
                : "7070_custom_game_ready_up_run_ack"));
        return true;
    }

    if (context.inner_emsg == 8052u) {
        const auto request = gbe::proto_wire::parse_dota8052_started_loading_request(body, body_size);
        if (request.lobby_id != 0 && request.lobby_id != local_lobby_snapshot.lobby_id)
            return true;
        {
            local_lobby.apply(context.wrapped ? "8052_wrapped_started_loading" : "8052_started_loading", [&request](GBE_LocalLobby &lobby) {
                gbe::dota_lobby_state::apply_custom_game_loading_metadata(
                    lobby,
                    request.custom_game_id,
                    static_cast<uint32>(request.start_time));
            });
        }

        request_state.has_launch_server_setup = gbe::dota_lobby_state::has_launch_server_setup_sync(local_lobby_snapshot);
        gbe::dota_lifecycle_state_machine::CustomGameRequest machine_request{};
        machine_request.event = event_mapping.event;
        execute_decision(gbe::dota_custom_game_lifecycle::decide_started_loading(
            request_state,
            machine_request,
            local_lobby_snapshot,
            true,
            GBE_kDotaLaunchPhaseSetupSynced,
            GBE_kDotaLaunchPhaseRunQueued,
            context.wrapped ? "8052_wrapped_started_loading" : "8052_started_loading",
            context.wrapped
                ? "custom game wrapped 8052 started loading"
                : "custom game 8052 started loading"));
        return true;
    }

    const auto load_result = gbe::proto_wire::parse_dota8053_finished_loading_request(body, body_size);
    if (load_result.lobby_id != 0 && load_result.lobby_id != local_lobby_snapshot.lobby_id)
        return true;
    const bool load_failed = gbe::proto_wire::dota8053_indicates_load_failure(load_result.result_code, load_result.result_text);
    gbe::dota_lifecycle_state_machine::CustomGameRequest machine_request{};
    machine_request.event = event_mapping.event;
    machine_request.load_failed = load_failed;
    const char *reason = context.wrapped
        ? (load_failed ? "8053_wrapped_load_failed" : "8053_wrapped_finished_loading")
        : (load_failed ? "8053_load_failed" : "8053_finished_loading");
    execute_decision(gbe::dota_custom_game_lifecycle::decide_finished_loading(
        request_state,
        machine_request,
        local_lobby_snapshot,
        true,
        load_failed,
        GBE_kDotaLaunchPhaseSetupSynced,
        GBE_kDotaLaunchPhaseRunQueued,
        GBE_kDotaLaunchPhaseLoaded,
        reason));
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Applied %s 8053 lobby_id=%llu loading_duration=%llu result_code=%llu signon_states=%llu load_failed=%u result_text=%s",
        context.wrapped ? "wrapped" : "direct",
        static_cast<unsigned long long>(local_lobby_snapshot.lobby_id),
        static_cast<unsigned long long>(load_result.loading_duration),
        static_cast<unsigned long long>(load_result.result_code),
        static_cast<unsigned long long>(load_result.signon_states),
        load_failed ? 1u : 0u,
        load_result.result_text.c_str());
    return true;
}
