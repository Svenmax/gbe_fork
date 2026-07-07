#ifndef GBE_DOTA_LOBBY_LAUNCH_FLOW_H
#define GBE_DOTA_LOBBY_LAUNCH_FLOW_H

#include "gbe_dota_action_model.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_types.h"

#include <cstdint>
#include <vector>

namespace gbe::dota_lobby_flow {

bool should_use_client_peer_for_launch_state_push(
    bool source_is_server,
    bool client_peer_available);

bool is_valid_launch_state_push_target(
    bool target_available,
    bool target_is_server,
    bool target_is_dota_profile);

bool should_preserve_server_id_for_launch_state_push_target(
    std::uint64_t target_local_steam_id,
    std::uint64_t lobby_owner_steam_id,
    bool lobby_lan,
    std::uint64_t lobby_match_id);

struct LaunchStatePushPlanInput
{
    bool source_lobby_suppressed{};
    bool source_is_server{};
    bool client_peer_available{};
    bool target_available{};
    bool target_is_server{};
    bool target_is_dota_profile{};
    bool shared_lobby_suppressed{};
    bool captured_lobby_active{};
    std::uint32_t lobby_state{};
    std::uint32_t lobby_game_state{};
    std::uint64_t lobby_server_id{};
    bool lobby_connect_available{};
    std::uint32_t last_pushed_game_state{};
    std::uint64_t target_local_steam_id{};
    std::uint64_t lobby_owner_steam_id{};
    bool lobby_lan{};
    std::uint64_t lobby_match_id{};
};

struct LaunchStatePushContext
{
    bool source_lobby_suppressed{};
    bool source_is_server{};
    bool client_peer_available{};
    bool target_available{};
    bool target_is_server{};
    bool target_is_dota_profile{};
    bool shared_lobby_suppressed{};
    bool captured_lobby_active{};
    GBE_LocalLobby captured_lobby{};
    std::uint32_t last_pushed_game_state{};
    std::uint64_t target_local_steam_id{};
};

struct LaunchStateCapturedLobbyInput
{
    bool active{};
    std::uint32_t state{};
    std::uint32_t game_state{};
    std::uint64_t server_id{};
    bool connect_available{};
    std::uint64_t owner_steam_id{};
    bool lan{};
    std::uint64_t match_id{};
};

struct LaunchStateTargetInput
{
    bool source_is_server{};
    bool client_peer_available{};
    bool target_available{};
    bool target_is_server{};
    bool target_is_dota_profile{};
};

struct LaunchStateSharedLobbyInput
{
    bool suppressed{};
};

struct LaunchStateCaptureInput
{
    bool active{};
};

struct LaunchStateCapturedContextInput
{
    std::uint32_t last_pushed_game_state{};
    std::uint64_t target_local_steam_id{};
};

struct LaunchStateSourceLobbyInput
{
    bool suppressed{};
};

void apply_source_lobby_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateSourceLobbyInput &source_lobby);

void apply_target_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateTargetInput &target);

void apply_shared_lobby_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateSharedLobbyInput &shared_lobby);

void apply_capture_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateCaptureInput &capture);

LaunchStateCapturedLobbyInput captured_lobby_input_from_local_lobby(
    const GBE_LocalLobby &lobby,
    bool captured_active);

LaunchStatePushPlanInput launch_state_push_plan_input_from_context(
    const LaunchStatePushContext &context);

void apply_captured_lobby_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateCapturedLobbyInput &lobby,
    const LaunchStateCapturedContextInput &context);

enum class LaunchStatePushSkipReason
{
    None,
    SuppressedSourceLobby,
    InvalidTarget,
    SuppressedSharedLobby,
    NoCapturedLobby,
    IneligibleLaunchState,
    DuplicateGameState,
};

enum class LaunchStatePushAction
{
    RecordCacheSubscription,
    PushCacheSubscribed,
    PushDetailsUpdate,
    ReapplyRichPresence,
    SetLastGameState,
};

enum class LaunchStatePayloadBuild
{
    CacheSubscribed,
    DetailsUpdate,
};

struct LaunchStatePayloadBuildRequest
{
    LaunchStatePayloadBuild build{};
    bool preserve_server_id{};
};

struct LaunchStatePushPlan
{
    bool use_client_peer{};
    bool restore_shared_state{};
    bool build_cache_subscribed{};
    bool build_details_update{};
    bool record_cache_subscription{};
    bool push_cache_subscribed{};
    bool push_details_update{};
    bool reapply_rich_presence{};
    bool set_last_game_state{};
    bool preserve_server_id{};
    LaunchStatePushSkipReason skip_reason{LaunchStatePushSkipReason::None};
};

LaunchStatePushPlan plan_launch_state_push(
    const LaunchStatePushPlanInput &input);

LaunchStatePushPlan plan_launch_state_push(
    const LaunchStatePushContext &context);

std::vector<LaunchStatePushAction> launch_state_push_actions(
    const LaunchStatePushPlan &plan);

GBE_DotaActionList launch_state_push_action_list(
    const LaunchStatePushPlan &plan);

std::vector<LaunchStatePayloadBuild> launch_state_payload_builds(
    const LaunchStatePushPlan &plan);

std::vector<LaunchStatePayloadBuildRequest> launch_state_payload_build_requests(
    const LaunchStatePushPlan &plan);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_LAUNCH_FLOW_H
