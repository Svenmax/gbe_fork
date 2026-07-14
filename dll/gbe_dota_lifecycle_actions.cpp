#include "gbe_dota_lifecycle_actions.h"
#include "gbe_dota_lifecycle_state_machine.h"

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

    if (effects.publish_local_member_data) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::LobbyLocalMemberData;
        action.reason = transition.reason;
        actions.push_back(std::move(action));
    }

    if (transition.publish_shared_state || effects.fallback_publish_on_runtime_update_failure) {
        GBE_DotaAction action;
        action.type = GBE_DotaActionType::SharedLobbyPublish;
        action.reason = transition.reason;
        action.only_when_runtime_update_not_queued = effects.fallback_publish_on_runtime_update_failure;
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

GBE_DotaActionList build_member_runtime_actions(
    std::uint64_t steam_id,
    bool connected,
    std::uint32_t hero_id,
    bool has_hero_id,
    const char *reason)
{
    GBE_DotaActionList actions;

    GBE_DotaAction update;
    update.type = GBE_DotaActionType::LobbyMemberRuntimeUpdate;
    update.target_steam_id = steam_id;
    update.connected = connected;
    update.hero_id = hero_id;
    update.has_hero_id = has_hero_id;
    update.reason = reason ? reason : "";
    actions.push_back(std::move(update));

    GBE_DotaAction publish;
    publish.type = GBE_DotaActionType::SharedLobbyPublish;
    publish.reason = reason ? reason : "";
    publish.only_when_previous_action_succeeded = true;
    actions.push_back(std::move(publish));

    return actions;
}

GBE_DotaActionList decide_member_runtime_actions(
    std::uint64_t generation,
    std::uint64_t steam_id,
    bool connected,
    std::uint32_t hero_id,
    bool has_hero_id,
    const char *reason)
{
    gbe::dota_lifecycle_state_machine::MachineState machine_state{};
    machine_state.generation = generation;
    const auto transition = gbe::dota_lifecycle_state_machine::transition_runtime_member(
        machine_state,
        { generation, steam_id, connected, hero_id, has_hero_id });
    if (!transition.accepted() || !transition.effects.contains(
            gbe::dota_lifecycle_state_machine::EffectKind::RuntimeMemberUpdateRequested))
        return {};
    return build_member_runtime_actions(steam_id, connected, hero_id, has_hero_id, reason);
}

} // namespace gbe::dota_lifecycle
