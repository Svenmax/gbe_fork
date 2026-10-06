#ifndef __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__
#define __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__

#include "gbe_dota_action_model.h"
#include "gbe_dota_lobby_state.h"

#include <cstdint>

class Steam_Game_Coordinator;

namespace gbe::dota_lifecycle {

enum class PushRoute {
    Immediate,
    DotaResponse,
    CacheUnsubscribedResponse,
};

struct ExecutionOptions {
    bool wrapped{};
    const std::string *outer_session_field_raw{};
    PushRoute push_route{ PushRoute::Immediate };
    const char *push_reason_override{};
    bool abort_on_push_failure{};
    Steam_Game_Coordinator *client_target{};
    const GBE_LocalLobby *client_lobby_restore{};
    bool mirror_launch_peripheral_to_client_target{};
    bool route_rich_presence_to_client_target{};
    bool route_push_to_client_target{};
};

struct ExecutionResult {
    bool runtime_update_queued{};
    bool state_changed{};
    bool details_update_sent{};
    bool succeeded{ true };
};

class Executor {
public:
    virtual ~Executor() = default;
    virtual ExecutionResult execute(
        Steam_Game_Coordinator &coordinator,
        const GBE_DotaActionList &actions,
        const ExecutionOptions &options) = 0;
};

class CoordinatorExecutor final : public Executor {
public:
    ExecutionResult execute(
        Steam_Game_Coordinator &coordinator,
        const GBE_DotaActionList &actions,
        const ExecutionOptions &options) override;
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
    bool local_lifecycle_pre_write{};
};

GBE_DotaActionList build_transition_actions(const TransitionEffects &effects);
GBE_DotaActionList build_member_runtime_actions(
    std::uint64_t steam_id,
    bool connected,
    std::uint32_t hero_id,
    bool has_hero_id,
    const char *reason);

// C8: single entry for member runtime — SM gate + action list (empty when rejected).
GBE_DotaActionList decide_member_runtime_actions(
    std::uint64_t generation,
    std::uint64_t steam_id,
    bool connected,
    std::uint32_t hero_id,
    bool has_hero_id,
    const char *reason);

} // namespace gbe::dota_lifecycle

#endif // __INCLUDED_GBE_DOTA_LIFECYCLE_ACTIONS_H__
