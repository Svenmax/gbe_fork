/* Wrapped custom-game lifecycle handlers for the Dota Game Coordinator. */

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_game_lifecycle.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"

bool Steam_Game_Coordinator::GBE_HandleDotaWrappedCustomGameLifecycleRequest(const gbe::dota_gc_router::DotaGcRequestContext &context)
{
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received wrapped custom-game lifecycle emsg=%u has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
        context.inner_emsg,
        context.has_request_job ? 1 : 0,
        static_cast<unsigned long long>(context.request_job_id),
        context.outer_session_field_raw.size(),
        gbe::proto_wire::format_hex_prefix(
            reinterpret_cast<const std::uint8_t *>(context.body.data()),
            context.body.size(),
            48).c_str());

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || !gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game))
        return true;

    const uint8 *body = reinterpret_cast<const uint8 *>(context.body.data());
    const size_t body_size = context.body.size();

    if (context.inner_emsg == 7070u) {
        uint32 ready_state = 0u;
        gbe::proto_wire::read_uint32_field(body, body_size, 1u, ready_state);
        std::string response_7170;
        if (gbe::gc_message::build_dota_ready_up_status_payload(context.has_request_job, context.request_job_id, GBE_local_lobby.lobby_id, 0u, ready_state != 0u ? ready_state : 1u, response_7170))
            GBE_PushDotaResponse(7170u, response_7170, true, &context.outer_session_field_raw, "7070_ready_up_status");

        const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision ready_up =
            gbe::dota_lobby_state::compute_custom_game_ready_up_transition(
                GBE_local_lobby, ready_state, GBE_kDotaLaunchPhaseRunQueued,
                "7070_wrapped_custom_game_ready_up_run_ack");
        if (ready_up.apply_lobby_state) {
            gbe::dota_custom_game_lifecycle::ExecutionContext execution{};
            execution.transition = ready_up;
            execution.wrapped = true;
            execution.outer_session_field_raw = &context.outer_session_field_raw;
            GBE_ExecuteDotaCustomGameLifecycleTransition(execution);
        }
        return true;
    }

    uint64 lobby_id = 0;
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, lobby_id);
    if (lobby_id != 0 && lobby_id != GBE_local_lobby.lobby_id)
        return true;

    if (context.inner_emsg == 8052u) {
        uint64 custom_game_id = 0;
        uint64 start_time = 0;
        gbe::proto_wire::read_uint64_field(body, body_size, 2u, custom_game_id);
        gbe::proto_wire::read_uint64_field(body, body_size, 4u, start_time);
        if (custom_game_id != 0)
            GBE_local_lobby.custom_game.game_id = custom_game_id;
        if (start_time != 0)
            GBE_local_lobby.game_start_time = static_cast<uint32>(start_time);

        const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision started_loading =
            gbe::dota_lobby_state::compute_custom_game_started_loading_transition(
                GBE_local_lobby, true, GBE_kDotaLaunchPhaseSetupSynced,
                GBE_kDotaLaunchPhaseRunQueued, "8052_wrapped_started_loading");
        gbe::dota_custom_game_lifecycle::ExecutionContext execution{};
        execution.transition = started_loading;
        execution.wrapped = true;
        execution.outer_session_field_raw = &context.outer_session_field_raw;
        execution.trigger_emsg = context.inner_emsg;
        execution.source_job = context.request_job_id;
        execution.runtime_update_note = "custom game wrapped 8052 started loading";
        GBE_ExecuteDotaCustomGameLifecycleTransition(execution);
        return true;
    }

    const gbe::proto_wire::Dota8053Result load_result = gbe::proto_wire::parse_dota8053_result(body, body_size);
    const bool load_failed = gbe::proto_wire::dota8053_indicates_load_failure(load_result.result_code, load_result.result_text);
    const char *reason = load_failed ? "8053_wrapped_load_failed" : "8053_wrapped_finished_loading";
    const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision finished_loading =
        gbe::dota_lobby_state::compute_custom_game_finished_loading_transition(
            GBE_local_lobby, true, load_failed, GBE_kDotaLaunchPhaseRunQueued,
            GBE_kDotaLaunchPhaseLoaded, reason);
    gbe::dota_custom_game_lifecycle::ExecutionContext execution{};
    execution.transition = finished_loading;
    execution.wrapped = true;
    execution.outer_session_field_raw = &context.outer_session_field_raw;
    execution.trigger_emsg = context.inner_emsg;
    execution.source_job = context.request_job_id;
    execution.update_local_member_runtime = !load_failed;
    execution.publish_local_member_data = !load_failed;
    GBE_ExecuteDotaCustomGameLifecycleTransition(execution);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Applied wrapped 8053 lobby_id=%llu loading_duration=%llu result_code=%llu signon_states=%llu load_failed=%u result_text=%s",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(load_result.loading_duration),
        static_cast<unsigned long long>(load_result.result_code),
        static_cast<unsigned long long>(load_result.signon_states),
        load_failed ? 1u : 0u,
        load_result.result_text.c_str());
    return true;
}
