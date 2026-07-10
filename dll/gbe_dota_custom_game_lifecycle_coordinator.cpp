/* Shared custom-game lifecycle side-effect executor. */

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_custom_game_lifecycle.h"

bool Steam_Game_Coordinator::GBE_ExecuteDotaCustomGameLifecycleTransition(
    const gbe::dota_custom_game_lifecycle::ExecutionContext &context)
{
    const auto &transition = context.transition;
    if (transition.apply_lobby_state && !transition.queue_runtime_lobby_update) {
        GBE_local_lobby.state = transition.next_state;
        GBE_local_lobby.game_state = transition.next_game_state;
    }

    if (context.update_local_member_runtime) {
        const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
        if (local_steam_id != 0ull)
            GBE_SetDotaLobbyMemberRuntimeState(local_steam_id, true, 0u, false);
    }

    bool runtime_update_queued = false;
    if (transition.queue_runtime_lobby_update) {
        if (transition.mark_launch_phase)
            GBE_MarkDotaLaunchPhase(transition.launch_phase, transition.reason.c_str());
        runtime_update_queued = GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(
            context.runtime_update_note,
            context.trigger_emsg,
            context.source_job,
            transition.next_state,
            transition.next_game_state,
            transition.runtime_update_delay);
    } else if (transition.mark_launch_phase) {
        GBE_MarkDotaLaunchPhase(transition.launch_phase, transition.reason.c_str());
    }

    if (context.publish_local_member_data)
        GBE_PublishDotaPracticeLobbyLocalMemberData(transition.reason.c_str());

    if (!runtime_update_queued && (transition.publish_shared_state || transition.queue_runtime_lobby_update))
        GBE_PublishSharedDotaLobbyState(transition.reason.c_str());
    if (!runtime_update_queued && transition.send_details_update)
        GBE_SendDotaPracticeLobbyDetailsUpdate(
            context.wrapped,
            context.outer_session_field_raw,
            transition.reason.c_str());
    return runtime_update_queued;
}
