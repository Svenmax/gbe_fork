/* Shared custom-game lifecycle side-effect executor. */

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_custom_game_lifecycle.h"

gbe::dota_lifecycle::ExecutionResult Steam_Game_Coordinator::GBE_ExecuteDotaLifecycleActions(
    const GBE_DotaActionList &actions,
    bool wrapped,
    const std::string *outer_session_field_raw)
{
    gbe::dota_lifecycle::ExecutionResult result;
    for (const GBE_DotaAction &action : actions) {
        if (action.only_when_runtime_update_not_queued && result.runtime_update_queued)
            continue;

        switch (action.type) {
            case GBE_DotaActionType::LobbyStateApply:
                GBE_local_lobby.state = action.lobby_state;
                GBE_local_lobby.game_state = action.lobby_game_state;
                break;
            case GBE_DotaActionType::LobbyMemberRuntimeUpdate:
                GBE_SetDotaLobbyMemberRuntimeState(
                    action.target_steam_id,
                    action.connected,
                    action.hero_id,
                    action.has_hero_id);
                break;
            case GBE_DotaActionType::LaunchPhaseMark:
                GBE_MarkDotaLaunchPhase(action.launch_phase, action.reason.c_str(), false);
                break;
            case GBE_DotaActionType::LobbyLocalMemberData:
                GBE_PublishDotaPracticeLobbyLocalMemberData(action.reason.c_str());
                break;
            case GBE_DotaActionType::RuntimeLobbyDetailsUpdate:
                result.runtime_update_queued = GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(
                    action.reason.c_str(),
                    action.emsg,
                    action.job_id,
                    action.lobby_state,
                    action.lobby_game_state,
                    action.delay);
                break;
            case GBE_DotaActionType::SharedLobbyPublish:
                GBE_PublishSharedDotaLobbyState(action.reason.c_str());
                break;
            case GBE_DotaActionType::PracticeLobbyDetailsUpdate:
                GBE_SendDotaPracticeLobbyDetailsUpdate(
                    wrapped,
                    outer_session_field_raw,
                    action.reason.c_str());
                break;
            default:
                break;
        }
    }
    return result;
}

bool Steam_Game_Coordinator::GBE_ExecuteDotaCustomGameLifecycleTransition(
    const gbe::dota_custom_game_lifecycle::ExecutionContext &context)
{
    gbe::dota_lifecycle::TransitionEffects effects;
    effects.transition = context.transition;
    effects.local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    effects.trigger_emsg = context.trigger_emsg;
    effects.source_job = context.source_job;
    effects.runtime_update_note = context.runtime_update_note;
    effects.update_local_member_runtime = context.update_local_member_runtime;
    effects.publish_local_member_data = context.publish_local_member_data;

    return GBE_ExecuteDotaLifecycleActions(
        gbe::dota_lifecycle::build_transition_actions(effects),
        context.wrapped,
        context.outer_session_field_raw).runtime_update_queued;
}
