#ifndef __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__
#define __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__

#include "gbe_dota_action_model.h"
#include "gbe_dota_lobby_state.h"

#include <cstdint>

namespace gbe::dota_lifecycle {

struct ExecutionResult {
    bool runtime_update_queued{};
    bool state_changed{};
    bool details_update_sent{};
};

struct TransitionEffects {
    gbe::dota_lobby_state::LaunchLifecycleTransitionDecision transition;
    std::uint64_t local_steam_id{};
    std::uint32_t trigger_emsg{};
    std::uint64_t source_job{};
    const char *runtime_update_note{};
    bool update_local_member_runtime{};
    bool publish_local_member_data{};
    bool fallback_publish_on_runtime_update_failure{};
};

GBE_DotaActionList build_transition_actions(const TransitionEffects &effects);
GBE_DotaActionList build_member_runtime_actions(
    std::uint64_t steam_id,
    bool connected,
    std::uint32_t hero_id,
    bool has_hero_id,
    const char *reason);

} // namespace gbe::dota_lifecycle

#endif // __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__
