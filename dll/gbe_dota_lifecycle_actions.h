#ifndef __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__
#define __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__

#include "gbe_dota_action_model.h"
#include "gbe_dota_lobby_state.h"

#include <cstdint>

namespace gbe::dota_lifecycle {

struct ExecutionResult {
    bool runtime_update_queued{};
};

struct TransitionEffects {
    gbe::dota_lobby_state::LaunchLifecycleTransitionDecision transition;
    std::uint64_t local_steam_id{};
    std::uint32_t trigger_emsg{};
    std::uint64_t source_job{};
    const char *runtime_update_note{};
    bool update_local_member_runtime{};
    bool publish_local_member_data{};
};

GBE_DotaActionList build_transition_actions(const TransitionEffects &effects);

} // namespace gbe::dota_lifecycle

#endif // __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__
