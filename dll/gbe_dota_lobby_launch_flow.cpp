#include "gbe_dota_lobby_launch_flow.h"

#include "gbe_dota_protocol_constants.h"

namespace gbe::dota_lobby_flow {

bool should_use_client_peer_for_launch_state_push(
    bool source_is_server,
    bool client_peer_available)
{
    return source_is_server && client_peer_available;
}

bool is_valid_launch_state_push_target(
    bool target_available,
    bool target_is_server,
    bool target_is_dota_profile)
{
    return target_available && !target_is_server && target_is_dota_profile;
}

bool should_preserve_server_id_for_launch_state_push_target(
    std::uint64_t target_local_steam_id,
    std::uint64_t lobby_owner_steam_id,
    bool lobby_lan,
    std::uint64_t lobby_match_id)
{
    return target_local_steam_id != 0ull &&
        lobby_owner_steam_id != 0ull &&
        target_local_steam_id == lobby_owner_steam_id &&
        lobby_lan &&
        lobby_match_id != 0ull;
}

bool should_use_current_practice_lobby_payload_for_details_update(
    std::uint64_t match_id,
    bool lan,
    std::size_t member_count,
    std::uint64_t custom_game_id)
{
    if (custom_game_id != 0ull)
        return true;

    const bool launched_lan_with_remote_members = match_id != 0ull && lan && member_count > 1u;
    return (match_id == 0ull || launched_lan_with_remote_members) && member_count > 1u;
}

bool should_use_current_practice_lobby_payload_for_cache_subscribed(
    bool launch_started,
    bool lan,
    std::size_t member_count)
{
    const bool launched_lan_with_remote_members = launch_started && lan && member_count > 1u;
    return (!launch_started || launched_lan_with_remote_members) && member_count > 1u;
}

GBE_DotaAuthoritativeLobbyPayloadData compose_authoritative_lobby_payload_data(
    bool preserve_server_id,
    bool launch_started,
    std::uint64_t current_server_id,
    std::uint64_t server_candidate_id,
    const std::string &formatted_connect)
{
    GBE_DotaAuthoritativeLobbyPayloadData data{};
    data.connect = formatted_connect;
    if (preserve_server_id && launch_started)
        data.server_id = current_server_id != 0ull ? current_server_id : server_candidate_id;
    return data;
}

void apply_source_lobby_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateSourceLobbyInput &source_lobby)
{
    plan_input.source_lobby_suppressed = source_lobby.suppressed;
}

void apply_target_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateTargetInput &target)
{
    plan_input.source_is_server = target.source_is_server;
    plan_input.client_peer_available = target.client_peer_available;
    plan_input.target_available = target.target_available;
    plan_input.target_is_server = target.target_is_server;
    plan_input.target_is_dota_profile = target.target_is_dota_profile;
}

void apply_shared_lobby_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateSharedLobbyInput &shared_lobby)
{
    plan_input.shared_lobby_suppressed = shared_lobby.suppressed;
}

void apply_capture_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateCaptureInput &capture)
{
    plan_input.captured_lobby_active = capture.active;
}

LaunchStateCapturedLobbyInput captured_lobby_input_from_local_lobby(
    const GBE_LocalLobby &lobby,
    bool captured_active)
{
    return LaunchStateCapturedLobbyInput{
        captured_active,
        lobby.state,
        lobby.game_state,
        lobby.server_id,
        !lobby.connect.empty(),
        lobby.owner_steam_id,
        lobby.lan,
        lobby.match_id};
}

LaunchStatePushPlanInput launch_state_push_plan_input_from_context(
    const LaunchStatePushContext &context)
{
    LaunchStatePushPlanInput input{};
    input.source_lobby_suppressed = context.source_lobby_suppressed;
    input.source_is_server = context.source_is_server;
    input.client_peer_available = context.client_peer_available;
    input.target_available = context.target_available;
    input.target_is_server = context.target_is_server;
    input.target_is_dota_profile = context.target_is_dota_profile;
    input.shared_lobby_suppressed = context.shared_lobby_suppressed;
    input.captured_lobby_active = context.captured_lobby_active;
    input.lobby_state = context.captured_lobby.state;
    input.lobby_game_state = context.captured_lobby.game_state;
    input.lobby_server_id = context.captured_lobby.server_id;
    input.lobby_connect_available = !context.captured_lobby.connect.empty();
    input.last_pushed_game_state = context.last_pushed_game_state;
    input.target_local_steam_id = context.target_local_steam_id;
    input.lobby_owner_steam_id = context.captured_lobby.owner_steam_id;
    input.lobby_lan = context.captured_lobby.lan;
    input.lobby_match_id = context.captured_lobby.match_id;
    return input;
}

void apply_captured_lobby_to_launch_state_push_plan_input(
    LaunchStatePushPlanInput &plan_input,
    const LaunchStateCapturedLobbyInput &lobby,
    const LaunchStateCapturedContextInput &context)
{
    plan_input.captured_lobby_active = lobby.active;
    plan_input.lobby_state = lobby.state;
    plan_input.lobby_game_state = lobby.game_state;
    plan_input.lobby_server_id = lobby.server_id;
    plan_input.lobby_connect_available = lobby.connect_available;
    plan_input.last_pushed_game_state = context.last_pushed_game_state;
    plan_input.target_local_steam_id = context.target_local_steam_id;
    plan_input.lobby_owner_steam_id = lobby.owner_steam_id;
    plan_input.lobby_lan = lobby.lan;
    plan_input.lobby_match_id = lobby.match_id;
}

LaunchStatePushPlan plan_launch_state_push(
    const LaunchStatePushPlanInput &input)
{
    LaunchStatePushPlan plan{};
    if (input.source_lobby_suppressed) {
        plan.skip_reason = LaunchStatePushSkipReason::SuppressedSourceLobby;
        return plan;
    }

    plan.use_client_peer = should_use_client_peer_for_launch_state_push(
        input.source_is_server,
        input.client_peer_available);

    if (!is_valid_launch_state_push_target(
            input.target_available,
            input.target_is_server,
            input.target_is_dota_profile)) {
        plan.skip_reason = LaunchStatePushSkipReason::InvalidTarget;
        return plan;
    }

    plan.restore_shared_state = true;

    if (input.shared_lobby_suppressed) {
        plan.skip_reason = LaunchStatePushSkipReason::SuppressedSharedLobby;
        return plan;
    }

    if (!input.captured_lobby_active) {
        plan.skip_reason = LaunchStatePushSkipReason::NoCapturedLobby;
        return plan;
    }

    const bool has_launch_endpoint = input.lobby_server_id != 0ull || input.lobby_connect_available;
    if (input.lobby_state != 2u || input.lobby_game_state < 1u || !has_launch_endpoint) {
        plan.skip_reason = LaunchStatePushSkipReason::IneligibleLaunchState;
        return plan;
    }

    if (input.last_pushed_game_state >= input.lobby_game_state) {
        plan.skip_reason = LaunchStatePushSkipReason::DuplicateGameState;
        return plan;
    }

    plan.build_cache_subscribed = true;
    plan.build_details_update = true;
    plan.record_cache_subscription = true;
    plan.push_cache_subscribed = true;
    plan.push_details_update = true;
    plan.reapply_rich_presence = true;
    plan.set_last_game_state = true;
    plan.preserve_server_id = should_preserve_server_id_for_launch_state_push_target(
        input.target_local_steam_id,
        input.lobby_owner_steam_id,
        input.lobby_lan,
        input.lobby_match_id);

    return plan;
}

LaunchStatePushPlan plan_launch_state_push(
    const LaunchStatePushContext &context)
{
    return plan_launch_state_push(launch_state_push_plan_input_from_context(context));
}

std::vector<LaunchStatePushAction> launch_state_push_actions(
    const LaunchStatePushPlan &plan)
{
    std::vector<LaunchStatePushAction> actions;
    if (plan.skip_reason != LaunchStatePushSkipReason::None)
        return actions;

    if (plan.record_cache_subscription)
        actions.push_back(LaunchStatePushAction::RecordCacheSubscription);
    if (plan.push_cache_subscribed)
        actions.push_back(LaunchStatePushAction::PushCacheSubscribed);
    if (plan.push_details_update)
        actions.push_back(LaunchStatePushAction::PushDetailsUpdate);
    if (plan.reapply_rich_presence)
        actions.push_back(LaunchStatePushAction::ReapplyRichPresence);
    if (plan.set_last_game_state)
        actions.push_back(LaunchStatePushAction::SetLastGameState);

    return actions;
}

GBE_DotaActionList launch_state_push_action_list(
    const LaunchStatePushPlan &plan)
{
    GBE_DotaActionList actions;
    for (LaunchStatePushAction action : launch_state_push_actions(plan)) {
        switch (action) {
            case LaunchStatePushAction::RecordCacheSubscription:
                actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbyCacheSubscriptionRecord });
                break;
            case LaunchStatePushAction::PushCacheSubscribed:
                actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheSubscribed | GBE_kProtoMask });
                break;
            case LaunchStatePushAction::PushDetailsUpdate:
                actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask });
                break;
            case LaunchStatePushAction::ReapplyRichPresence:
                actions.push_back(GBE_DotaAction{ GBE_DotaActionType::RichPresenceUpdate });
                break;
            case LaunchStatePushAction::SetLastGameState:
                actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LaunchStateGameStateRecord });
                break;
        }
    }
    return actions;
}

std::vector<LaunchStatePayloadBuild> launch_state_payload_builds(
    const LaunchStatePushPlan &plan)
{
    std::vector<LaunchStatePayloadBuild> builds;
    if (plan.skip_reason != LaunchStatePushSkipReason::None)
        return builds;

    if (plan.build_cache_subscribed)
        builds.push_back(LaunchStatePayloadBuild::CacheSubscribed);
    if (plan.build_details_update)
        builds.push_back(LaunchStatePayloadBuild::DetailsUpdate);

    return builds;
}

std::vector<LaunchStatePayloadBuildRequest> launch_state_payload_build_requests(
    const LaunchStatePushPlan &plan)
{
    std::vector<LaunchStatePayloadBuildRequest> requests;
    for (LaunchStatePayloadBuild build : launch_state_payload_builds(plan))
        requests.push_back(LaunchStatePayloadBuildRequest{build, plan.preserve_server_id});
    return requests;
}

} // namespace gbe::dota_lobby_flow
