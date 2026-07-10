#include "gbe_dota_lifecycle_actions.h"

#include <utility>

namespace gbe::dota_lifecycle {

GBE_DotaActionList build_transition_actions(const TransitionEffects &effects)
{
    const auto &transition = effects.transition;
    GBE_DotaActionList actions;

    if (transition.apply_lobby_state && !transition.queue_runtime_lobby_update) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::LobbyStateApply;
        action.lobby_state = transition.next_state;
        action.lobby_game_state = transition.next_game_state;
        action.reason = transition.reason;
        actions.push_back(std::move(action));
    }

    if (effects.update_local_member_runtime && effects.local_steam_id != 0ull) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::LobbyMemberRuntimeUpdate;
        action.target_steam_id = effects.local_steam_id;
        action.connected = true;
        action.reason = transition.reason;
        actions.push_back(std::move(action));
    }

    if (transition.mark_launch_phase) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::LaunchPhaseMark;
        action.launch_phase = transition.launch_phase;
        action.reason = transition.reason;
        actions.push_back(std::move(action));
    }

    if (effects.publish_local_member_data) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::LobbyLocalMemberData;
        action.reason = transition.reason;
        actions.push_back(std::move(action));
    }

    if (transition.queue_runtime_lobby_update) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::RuntimeLobbyDetailsUpdate;
        action.emsg = effects.trigger_emsg;
        action.job_id = effects.source_job;
        action.lobby_state = transition.next_state;
        action.lobby_game_state = transition.next_game_state;
        action.delay = transition.runtime_update_delay;
        action.reason = effects.runtime_update_note ? effects.runtime_update_note : transition.reason;
        actions.push_back(std::move(action));
    }

    if (transition.publish_shared_state || transition.queue_runtime_lobby_update) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::SharedLobbyPublish;
        action.reason = transition.reason;
        action.only_when_runtime_update_not_queued = transition.queue_runtime_lobby_update;
        actions.push_back(std::move(action));
    }

    if (transition.send_details_update) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::PracticeLobbyDetailsUpdate;
        action.reason = transition.reason;
        action.only_when_runtime_update_not_queued = transition.queue_runtime_lobby_update;
        actions.push_back(std::move(action));
    }

    return actions;
}

} // namespace gbe::dota_lifecycle
