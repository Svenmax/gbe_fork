#ifndef __INCLUDED_GBE_DOTA_CUSTOM_GAME_LIFECYCLE_H__
#define __INCLUDED_GBE_DOTA_CUSTOM_GAME_LIFECYCLE_H__

#include "gbe_dota_lobby_state.h"
#include "gbe_dota_lifecycle_actions.h"
#include "gbe_dota_lifecycle_state_machine.h"

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

// C7: single decision entry — SM gate + compute_* in one place (handler only Execute).
struct LifecycleDecision {
    bool accepted{};
    gbe::dota_lobby_state::LaunchLifecycleTransitionDecision transition{};
    bool update_local_member_runtime{};
    bool publish_local_member_data{};
    const char *runtime_update_note{};
};

LifecycleDecision decide_ready_up(
    const gbe::dota_lifecycle_state_machine::CustomGameRequestState &request_state,
    const gbe::dota_lifecycle_state_machine::CustomGameRequest &machine_request,
    const GBE_LocalLobby &lobby,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason);

LifecycleDecision decide_started_loading(
    const gbe::dota_lifecycle_state_machine::CustomGameRequestState &request_state,
    const gbe::dota_lifecycle_state_machine::CustomGameRequest &machine_request,
    const GBE_LocalLobby &lobby,
    bool matching_lobby,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason,
    const char *runtime_update_note);

LifecycleDecision decide_finished_loading(
    const gbe::dota_lifecycle_state_machine::CustomGameRequestState &request_state,
    const gbe::dota_lifecycle_state_machine::CustomGameRequest &machine_request,
    const GBE_LocalLobby &lobby,
    bool matching_lobby,
    bool load_failed,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase,
    std::uint32_t loaded_launch_phase,
    const std::string &reason);

} // namespace gbe::dota_custom_game_lifecycle

#endif // __INCLUDED_GBE_DOTA_CUSTOM_GAME_LIFECYCLE_H__
