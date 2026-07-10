#ifndef __INCLUDED_GBE_DOTA_CUSTOM_GAME_LIFECYCLE_H__
#define __INCLUDED_GBE_DOTA_CUSTOM_GAME_LIFECYCLE_H__

#include "gbe_dota_lobby_state.h"
#include "gbe_dota_lifecycle_actions.h"

#include <cstdint>
#include <string>

namespace gbe::dota_custom_game_lifecycle {

struct ExecutionContext {
    gbe::dota_lobby_state::LaunchLifecycleTransitionDecision transition;
    bool wrapped{};
    const std::string *outer_session_field_raw{};
    std::uint32_t trigger_emsg{};
    std::uint64_t source_job{};
    const char *runtime_update_note{};
    bool update_local_member_runtime{};
    bool publish_local_member_data{};
};

} // namespace gbe::dota_custom_game_lifecycle

#endif // __INCLUDED_GBE_DOTA_CUSTOM_GAME_LIFECYCLE_H__
