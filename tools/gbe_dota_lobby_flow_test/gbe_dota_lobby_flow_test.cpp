#include "dll/gbe_dota_lobby_flow.h"
#include "dll/gbe_dota_types.h"

#include <iostream>

namespace {

constexpr std::uint32_t kGoodGuys = 0u;
constexpr std::uint32_t kPlayerPool = 4u;

GBE_DotaLobbyMemberState member(std::uint64_t steam_id, std::uint32_t slot)
{
    GBE_DotaLobbyMemberState value{};
    value.steam_id = steam_id;
    value.account_id = static_cast<std::uint32_t>(steam_id & 0xffffffffu);
    value.team = kGoodGuys;
    value.slot = slot;
    value.hero_id = slot + 10u;
    value.connected = true;
    return value;
}

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;

    std::cerr << "failed: " << label << std::endl;
    return false;
}

bool expect_eq_u64(std::uint64_t actual, std::uint64_t expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool expect_eq_skip_reason(
    gbe::dota_lobby_flow::LaunchStatePushSkipReason actual,
    gbe::dota_lobby_flow::LaunchStatePushSkipReason expected,
    const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << static_cast<int>(actual) << " expected=" << static_cast<int>(expected) << std::endl;
    return false;
}

bool expect_eq_action(
    gbe::dota_lobby_flow::LaunchStatePushAction actual,
    gbe::dota_lobby_flow::LaunchStatePushAction expected,
    const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << static_cast<int>(actual) << " expected=" << static_cast<int>(expected) << std::endl;
    return false;
}

bool expect_eq_payload_build(
    gbe::dota_lobby_flow::LaunchStatePayloadBuild actual,
    gbe::dota_lobby_flow::LaunchStatePayloadBuild expected,
    const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << static_cast<int>(actual) << " expected=" << static_cast<int>(expected) << std::endl;
    return false;
}

gbe::dota_lobby_flow::LaunchStatePushPlanInput valid_launch_state_plan_input()
{
    gbe::dota_lobby_flow::LaunchStatePushPlanInput input{};
    input.source_is_server = true;
    input.client_peer_available = true;
    input.target_available = true;
    input.target_is_server = false;
    input.target_is_dota_profile = true;
    input.shared_lobby_suppressed = false;
    input.captured_lobby_active = true;
    input.lobby_state = 2u;
    input.lobby_game_state = 3u;
    input.lobby_server_id = 99ull;
    input.lobby_connect_available = false;
    input.last_pushed_game_state = 2u;
    input.target_local_steam_id = 10ull;
    input.lobby_owner_steam_id = 10ull;
    input.lobby_lan = true;
    input.lobby_match_id = 123ull;
    return input;
}

bool test_member_find_and_equality()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u), member(20ull, 2u)};
    std::vector<GBE_DotaLobbyMemberState> same = members;
    std::vector<GBE_DotaLobbyMemberState> different = members;
    different[1].slot = 3u;

    ok &= expect_true(gbe::dota_lobby_flow::lobby_members_equal(members, same), "members equal");
    ok &= expect_true(!gbe::dota_lobby_flow::lobby_members_equal(members, different), "members differ");
    ok &= expect_true(gbe::dota_lobby_flow::lobby_members_contain_steam_id(members, 20ull), "contains steam id");
    ok &= expect_true(!gbe::dota_lobby_flow::lobby_members_contain_steam_id(members, 0ull), "reject zero steam id");

    std::size_t index = 0;
    ok &= expect_true(gbe::dota_lobby_flow::find_lobby_member_index(members, 20ull, index), "find member index");
    ok &= expect_eq_u64(index, 1u, "member index value");
    ok &= expect_true(!gbe::dota_lobby_flow::find_lobby_member_index(members, 30ull, index), "missing member index");

    ok &= expect_eq_u64(gbe::dota_lobby_flow::find_lobby_member_steam_id_by_account_id(members, 20u), 20ull, "find steam id by explicit account");
    members[1].account_id = 0u;
    ok &= expect_eq_u64(gbe::dota_lobby_flow::find_lobby_member_steam_id_by_account_id(members, 20u), 20ull, "find steam id by derived account");
    ok &= expect_eq_u64(gbe::dota_lobby_flow::find_lobby_member_steam_id_by_account_id(members, 0u), 0ull, "ignore zero account lookup");

    ok &= expect_true(gbe::dota_lobby_flow::clear_lobby_member_by_account_id(members, 20u), "clear member by account");
    ok &= expect_eq_u64(members.size(), 2u, "clear keeps member slots");
    ok &= expect_eq_u64(members[1].steam_id, 0ull, "clear zeroes member slot");
    ok &= expect_true(!gbe::dota_lobby_flow::clear_lobby_member_by_account_id(members, 20u), "clear missing member by account");

    return ok;
}

bool test_member_diff_and_filter()
{
    bool ok = true;

    std::vector<GBE_DotaLobbyMemberState> previous{member(10ull, 1u), member(20ull, 2u)};
    std::vector<GBE_DotaLobbyMemberState> current{member(10ull, 1u), GBE_DotaLobbyMemberState{}, member(30ull, 3u), member(30ull, 4u)};
    std::vector<GBE_DotaLobbyMemberState> joined = gbe::dota_lobby_flow::find_joined_lobby_members(previous, current);
    ok &= expect_eq_u64(joined.size(), 2u, "joined keeps repeated new members");
    ok &= expect_eq_u64(joined[0].steam_id, 30ull, "joined first steam id");
    ok &= expect_eq_u64(joined[0].slot, 3u, "joined first slot");
    ok &= expect_eq_u64(joined[1].steam_id, 30ull, "joined repeated steam id");
    ok &= expect_eq_u64(joined[1].slot, 4u, "joined repeated slot");

    std::vector<GBE_DotaLobbyMemberState> filtered = gbe::dota_lobby_flow::filter_nonzero_lobby_members(current);
    ok &= expect_eq_u64(filtered.size(), 3u, "filter removes zero members");
    ok &= expect_eq_u64(filtered[0].steam_id, 10ull, "filter keeps first member");
    ok &= expect_eq_u64(filtered[1].steam_id, 30ull, "filter keeps second member");
    ok &= expect_eq_u64(filtered[2].slot, 4u, "filter keeps repeated member order");

    return ok;
}

bool test_count_remote_lobby_members()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{GBE_DotaLobbyMemberState{}, member(10ull, 1u), member(20ull, 2u), member(30ull, 3u)};
    members[2].connected = false;

    std::uint32_t remote_count = 99u;
    std::uint32_t connected_remote_count = 99u;
    gbe::dota_lobby_flow::count_remote_lobby_members(members, 10ull, remote_count, connected_remote_count);
    ok &= expect_eq_u64(remote_count, 2u, "remote member count");
    ok &= expect_eq_u64(connected_remote_count, 1u, "connected remote member count");

    gbe::dota_lobby_flow::count_remote_lobby_members(members, 0ull, remote_count, connected_remote_count);
    ok &= expect_eq_u64(remote_count, 3u, "remote count with no owner");
    ok &= expect_eq_u64(connected_remote_count, 2u, "connected remote count with no owner");

    ok &= expect_true(gbe::dota_lobby_flow::should_hold_lan_launch_for_remote_members(members, 10ull, remote_count, connected_remote_count), "hold when remote member disconnected");
    ok &= expect_eq_u64(remote_count, 2u, "hold remote count");
    ok &= expect_eq_u64(connected_remote_count, 1u, "hold connected remote count");

    members[2].connected = true;
    ok &= expect_true(!gbe::dota_lobby_flow::should_hold_lan_launch_for_remote_members(members, 10ull, remote_count, connected_remote_count), "do not hold when all remotes connected");
    ok &= expect_eq_u64(remote_count, 2u, "ready remote count");
    ok &= expect_eq_u64(connected_remote_count, 2u, "ready connected remote count");

    std::vector<GBE_DotaLobbyMemberState> only_owner{member(10ull, 1u)};
    ok &= expect_true(!gbe::dota_lobby_flow::should_hold_lan_launch_for_remote_members(only_owner, 10ull, remote_count, connected_remote_count), "do not hold with no remotes");
    ok &= expect_eq_u64(remote_count, 0u, "no remote count");
    ok &= expect_eq_u64(connected_remote_count, 0u, "no connected remote count");

    return ok;
}

bool test_launch_state_peer_selection()
{
    bool ok = true;

    ok &= expect_true(gbe::dota_lobby_flow::should_use_client_peer_for_launch_state_push(true, true), "server uses available client peer");
    ok &= expect_true(!gbe::dota_lobby_flow::should_use_client_peer_for_launch_state_push(true, false), "server keeps self without client peer");
    ok &= expect_true(!gbe::dota_lobby_flow::should_use_client_peer_for_launch_state_push(false, true), "client keeps self even with peer available");
    ok &= expect_true(!gbe::dota_lobby_flow::should_use_client_peer_for_launch_state_push(false, false), "client keeps self without peer");

    ok &= expect_true(gbe::dota_lobby_flow::is_valid_launch_state_push_target(true, false, true), "client dota target valid");
    ok &= expect_true(!gbe::dota_lobby_flow::is_valid_launch_state_push_target(false, false, true), "missing target invalid");
    ok &= expect_true(!gbe::dota_lobby_flow::is_valid_launch_state_push_target(true, true, true), "server target invalid");
    ok &= expect_true(!gbe::dota_lobby_flow::is_valid_launch_state_push_target(true, false, false), "non dota target invalid");

    ok &= expect_true(gbe::dota_lobby_flow::should_preserve_server_id_for_launch_state_push_target(10ull, 10ull, true, 99ull), "owner lan launch preserves server id");
    ok &= expect_true(!gbe::dota_lobby_flow::should_preserve_server_id_for_launch_state_push_target(0ull, 10ull, true, 99ull), "zero target steam id skips preserve");
    ok &= expect_true(!gbe::dota_lobby_flow::should_preserve_server_id_for_launch_state_push_target(10ull, 0ull, true, 99ull), "zero owner steam id skips preserve");
    ok &= expect_true(!gbe::dota_lobby_flow::should_preserve_server_id_for_launch_state_push_target(10ull, 20ull, true, 99ull), "non owner target skips preserve");
    ok &= expect_true(!gbe::dota_lobby_flow::should_preserve_server_id_for_launch_state_push_target(10ull, 10ull, false, 99ull), "non lan lobby skips preserve");
    ok &= expect_true(!gbe::dota_lobby_flow::should_preserve_server_id_for_launch_state_push_target(10ull, 10ull, true, 0ull), "missing match skips preserve");

    return ok;
}

bool test_launch_state_push_planner()
{
    bool ok = true;

    gbe::dota_lobby_flow::LaunchStatePushPlanInput input = valid_launch_state_plan_input();
    gbe::dota_lobby_flow::LaunchStatePushPlan plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_true(plan.use_client_peer, "planner uses client peer");
    ok &= expect_true(plan.restore_shared_state, "planner restores shared state before push");
    ok &= expect_true(plan.build_cache_subscribed, "planner builds cache subscribed");
    ok &= expect_true(plan.build_details_update, "planner builds details update");
    ok &= expect_true(plan.record_cache_subscription, "planner records cache subscription");
    ok &= expect_true(plan.push_cache_subscribed, "planner pushes cache subscribed");
    ok &= expect_true(plan.push_details_update, "planner pushes details update");
    ok &= expect_true(plan.reapply_rich_presence, "planner reapplies rich presence");
    ok &= expect_true(plan.set_last_game_state, "planner updates last game state");
    ok &= expect_true(plan.preserve_server_id, "planner preserves owner lan server id");
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "planner push skip reason");

    input = valid_launch_state_plan_input();
    input.source_lobby_suppressed = true;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::SuppressedSourceLobby, "planner suppressed source lobby skip");
    ok &= expect_true(!plan.use_client_peer, "suppressed source lobby skips target selection");
    ok &= expect_true(!plan.restore_shared_state, "suppressed source lobby skips restore");

    input = valid_launch_state_plan_input();
    input.target_available = false;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::InvalidTarget, "planner invalid target skip");
    ok &= expect_true(!plan.restore_shared_state, "invalid target skips restore");

    input = valid_launch_state_plan_input();
    input.shared_lobby_suppressed = true;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::SuppressedSharedLobby, "planner suppressed shared lobby skip");
    ok &= expect_true(plan.restore_shared_state, "suppressed shared lobby occurs after restore");
    ok &= expect_true(!plan.push_cache_subscribed, "suppressed shared lobby skips push");

    input = valid_launch_state_plan_input();
    input.captured_lobby_active = false;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::NoCapturedLobby, "planner no captured lobby skip");

    input = valid_launch_state_plan_input();
    input.lobby_state = 1u;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::IneligibleLaunchState, "planner state ineligible skip");

    input = valid_launch_state_plan_input();
    input.lobby_server_id = 0ull;
    input.lobby_connect_available = false;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::IneligibleLaunchState, "planner missing endpoint skip");

    input = valid_launch_state_plan_input();
    input.last_pushed_game_state = 3u;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::DuplicateGameState, "planner duplicate game state skip");

    input = valid_launch_state_plan_input();
    input.lobby_server_id = 0ull;
    input.lobby_connect_available = true;
    input.target_local_steam_id = 20ull;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "planner connect endpoint push");
    ok &= expect_true(plan.push_details_update, "planner pushes details update with connect endpoint");
    ok &= expect_true(!plan.preserve_server_id, "planner skips preserve for non-owner target");

    std::vector<gbe::dota_lobby_flow::LaunchStatePushAction> actions = gbe::dota_lobby_flow::launch_state_push_actions(plan);
    ok &= expect_eq_u64(actions.size(), 5u, "planner action count");
    ok &= expect_eq_action(actions[0], gbe::dota_lobby_flow::LaunchStatePushAction::RecordCacheSubscription, "planner first action records cache subscription");
    ok &= expect_eq_action(actions[1], gbe::dota_lobby_flow::LaunchStatePushAction::PushCacheSubscribed, "planner second action pushes cache subscribed");
    ok &= expect_eq_action(actions[2], gbe::dota_lobby_flow::LaunchStatePushAction::PushDetailsUpdate, "planner third action pushes details update");
    ok &= expect_eq_action(actions[3], gbe::dota_lobby_flow::LaunchStatePushAction::ReapplyRichPresence, "planner fourth action reapplies rich presence");
    ok &= expect_eq_action(actions[4], gbe::dota_lobby_flow::LaunchStatePushAction::SetLastGameState, "planner fifth action updates last game state");

    GBE_DotaActionList action_list = gbe::dota_lobby_flow::launch_state_push_action_list(plan);
    ok &= expect_eq_u64(action_list.size(), 5u, "planner unified action count");
    ok &= expect_true(action_list[0].type == GBE_DotaActionType::LobbyCacheSubscriptionRecord, "planner unified first action records cache subscription");
    ok &= expect_true(action_list[1].type == GBE_DotaActionType::PushIncomingNow, "planner unified second action pushes cache subscribed");
    ok &= expect_eq_u64(action_list[1].emsg, 24u | 0x80000000u, "planner unified second action emsg");
    ok &= expect_true(action_list[2].type == GBE_DotaActionType::PushIncomingNow, "planner unified third action pushes details update");
    ok &= expect_eq_u64(action_list[2].emsg, 26u | 0x80000000u, "planner unified third action emsg");
    ok &= expect_true(action_list[3].type == GBE_DotaActionType::RichPresenceUpdate, "planner unified fourth action reapplies rich presence");
    ok &= expect_true(action_list[4].type == GBE_DotaActionType::LaunchStateGameStateRecord, "planner unified fifth action updates last game state");

    std::vector<gbe::dota_lobby_flow::LaunchStatePayloadBuild> builds = gbe::dota_lobby_flow::launch_state_payload_builds(plan);
    ok &= expect_eq_u64(builds.size(), 2u, "planner payload build count");
    ok &= expect_eq_payload_build(builds[0], gbe::dota_lobby_flow::LaunchStatePayloadBuild::CacheSubscribed, "planner first payload build creates cache subscribed");
    ok &= expect_eq_payload_build(builds[1], gbe::dota_lobby_flow::LaunchStatePayloadBuild::DetailsUpdate, "planner second payload build creates details update");

    std::vector<gbe::dota_lobby_flow::LaunchStatePayloadBuildRequest> build_requests = gbe::dota_lobby_flow::launch_state_payload_build_requests(plan);
    ok &= expect_eq_u64(build_requests.size(), 2u, "planner payload build request count");
    ok &= expect_eq_payload_build(build_requests[0].build, gbe::dota_lobby_flow::LaunchStatePayloadBuild::CacheSubscribed, "planner first payload build request creates cache subscribed");
    ok &= expect_true(!build_requests[0].preserve_server_id, "planner first payload build request skips server id preserve");
    ok &= expect_eq_payload_build(build_requests[1].build, gbe::dota_lobby_flow::LaunchStatePayloadBuild::DetailsUpdate, "planner second payload build request creates details update");
    ok &= expect_true(!build_requests[1].preserve_server_id, "planner second payload build request skips server id preserve");

    input = valid_launch_state_plan_input();
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    build_requests = gbe::dota_lobby_flow::launch_state_payload_build_requests(plan);
    ok &= expect_eq_u64(build_requests.size(), 2u, "planner preserve payload build request count");
    ok &= expect_true(build_requests[0].preserve_server_id, "planner first payload build request preserves server id");
    ok &= expect_true(build_requests[1].preserve_server_id, "planner second payload build request preserves server id");

    input = valid_launch_state_plan_input();
    input.target_available = false;
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    actions = gbe::dota_lobby_flow::launch_state_push_actions(plan);
    ok &= expect_eq_u64(actions.size(), 0u, "planner skipped action count");
    builds = gbe::dota_lobby_flow::launch_state_payload_builds(plan);
    ok &= expect_eq_u64(builds.size(), 0u, "planner skipped payload build count");
    build_requests = gbe::dota_lobby_flow::launch_state_payload_build_requests(plan);
    ok &= expect_eq_u64(build_requests.size(), 0u, "planner skipped payload build request count");
    action_list = gbe::dota_lobby_flow::launch_state_push_action_list(plan);
    ok &= expect_eq_u64(action_list.size(), 0u, "planner skipped unified action count");

    return ok;
}

bool test_launch_state_push_context_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::LaunchStatePushContext context{};
    context.source_is_server = true;
    context.client_peer_available = true;
    context.target_available = true;
    context.target_is_server = false;
    context.target_is_dota_profile = true;
    context.captured_lobby_active = true;
    context.captured_lobby.state = 2u;
    context.captured_lobby.game_state = 3u;
    context.captured_lobby.server_id = 99ull;
    context.captured_lobby.owner_steam_id = 10ull;
    context.captured_lobby.lan = true;
    context.captured_lobby.match_id = 123ull;
    context.last_pushed_game_state = 2u;
    context.target_local_steam_id = 10ull;

    gbe::dota_lobby_flow::LaunchStatePushPlanInput input = gbe::dota_lobby_flow::launch_state_push_plan_input_from_context(context);
    ok &= expect_true(input.source_is_server, "context mapping source server");
    ok &= expect_true(input.client_peer_available, "context mapping client peer available");
    ok &= expect_true(input.target_available, "context mapping target available");
    ok &= expect_true(!input.target_is_server, "context mapping target client");
    ok &= expect_true(input.target_is_dota_profile, "context mapping target dota profile");
    ok &= expect_true(input.captured_lobby_active, "context mapping captured lobby active");
    ok &= expect_eq_u64(input.lobby_state, 2u, "context mapping lobby state");
    ok &= expect_eq_u64(input.lobby_game_state, 3u, "context mapping lobby game state");
    ok &= expect_eq_u64(input.lobby_server_id, 99ull, "context mapping server id");
    ok &= expect_true(!input.lobby_connect_available, "context mapping missing connect");
    ok &= expect_eq_u64(input.last_pushed_game_state, 2u, "context mapping last pushed game state");
    ok &= expect_eq_u64(input.target_local_steam_id, 10ull, "context mapping target local steam id");
    ok &= expect_eq_u64(input.lobby_owner_steam_id, 10ull, "context mapping owner steam id");
    ok &= expect_true(input.lobby_lan, "context mapping lan flag");
    ok &= expect_eq_u64(input.lobby_match_id, 123ull, "context mapping match id");

    gbe::dota_lobby_flow::LaunchStatePushPlan plan = gbe::dota_lobby_flow::plan_launch_state_push(context);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "context mapping planner push");
    ok &= expect_true(plan.preserve_server_id, "context mapping planner preserves server id");

    context.captured_lobby.server_id = 0ull;
    context.captured_lobby.connect = "127.0.0.1:27015";
    input = gbe::dota_lobby_flow::launch_state_push_plan_input_from_context(context);
    ok &= expect_true(input.lobby_connect_available, "context mapping connect endpoint");
    plan = gbe::dota_lobby_flow::plan_launch_state_push(context);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "context mapping connect planner push");

    return ok;
}

bool test_create_lobby_context_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::CreateLobbyContext context{};
    context.previous_lobby.active = true;
    context.previous_lobby.lobby_id = 0x7000ull;
    context.previous_lobby.match_id = 0x8000ull;
    context.pre_reset_custom_game.game_id = 0x9000ull;
    context.request.has_lobby_details = true;
    context.request.lobby_details.has_room_name = true;
    context.request.lobby_details.room_name = "context-room";
    context.request.lobby_details.has_custom_game_id = true;
    context.request.lobby_details.custom_game_id = 0x9000ull;
    context.parsed_request = true;
    context.new_lobby_id = 0x7100ull;
    context.owner_steam_id = 0x7200ull;
    context.owner_account_id = 72u;
    context.owner_name = "context-owner";
    context.owner_team = 0u;
    context.owner_slot = 1u;

    const gbe::dota_lobby_state::CreateLobbyResetPlan reset_plan = gbe::dota_lobby_flow::create_lobby_reset_plan_from_context(context);
    ok &= expect_true(reset_plan.unsubscribe_previous_practice_lobby, "create context reset unsubscribes previous lobby");
    ok &= expect_eq_u64(reset_plan.previous_lobby_id, 0x7000ull, "create context reset previous lobby id");
    ok &= expect_eq_u64(reset_plan.previous_match_id, 0x8000ull, "create context reset previous match id");

    const gbe::dota_lobby_flow::CreateLobbyActionPlan action_plan = gbe::dota_lobby_flow::create_lobby_action_plan_from_reset_plan(reset_plan);
    ok &= expect_true(action_plan.unsubscribe_previous_practice_lobby, "create context action plan unsubscribe");

    const gbe::dota_lobby_state::CreateLobbyPlan state_plan = gbe::dota_lobby_flow::create_lobby_state_plan_from_context(context);
    ok &= expect_eq_u64(state_plan.lobby.lobby_id, 0x7100ull, "create context state lobby id");
    ok &= expect_eq_u64(state_plan.lobby.owner_steam_id, 0x7200ull, "create context state owner steam id");
    ok &= expect_eq_u64(state_plan.lobby.owner_account_id, 72u, "create context state owner account id");
    ok &= expect_true(state_plan.lobby.owner_name == "context-owner", "create context state owner name");
    ok &= expect_true(state_plan.lobby.room_name == "context-room", "create context state room name");
    ok &= expect_eq_u64(state_plan.lobby.custom_game.game_id, 0x9000ull, "create context state custom game id");
    ok &= expect_true(state_plan.custom_game_create, "create context state custom flag");

    return ok;
}

bool test_create_lobby_action_list()
{
    bool ok = true;

    GBE_DotaActionList actions = gbe::dota_lobby_flow::create_lobby_action_list(gbe::dota_lobby_flow::CreateLobbyActionPlan{}, true);
    ok &= expect_eq_u64(actions.size(), 9u, "create action count");
    ok &= expect_true(actions[0].type == GBE_DotaActionType::GcMemoryReset, "create first action resets GC memory");
    ok &= expect_true(actions[0].reason == "7038_create", "create reset reason");
    ok &= expect_true(actions[1].type == GBE_DotaActionType::GenericLobbyCreate, "create second action creates generic lobby");
    ok &= expect_true(actions[1].reason == "7038_create", "create generic lobby reason");
    ok &= expect_true(actions[2].type == GBE_DotaActionType::LobbyLocalMemberData, "create third action publishes local member data");
    ok &= expect_true(actions[2].reason == "7038_create", "create local member reason");
    ok &= expect_true(actions[3].type == GBE_DotaActionType::SettingsLobbySync, "create fourth action syncs settings");
    ok &= expect_true(actions[3].reason == "7038_create", "create settings reason");
    ok &= expect_true(actions[4].type == GBE_DotaActionType::LobbySnapshotRefresh, "create fifth action publishes shared lobby");
    ok &= expect_true(actions[4].reason == "7038_create", "create shared reason");
    ok &= expect_true(actions[5].type == GBE_DotaActionType::LobbyMetadataPublish, "create sixth action publishes metadata");
    ok &= expect_true(actions[5].reason == "7038_create", "create metadata reason");
    ok &= expect_true(actions[6].type == GBE_DotaActionType::LobbyCacheSubscriptionRecord, "create seventh action records cache subscription");
    ok &= expect_true(actions[6].reason == "7038_create_wrapped", "create wrapped cache record reason");
    ok &= expect_true(actions[7].type == GBE_DotaActionType::PushIncomingNow, "create eighth action pushes cache subscribed");
    ok &= expect_eq_u64(actions[7].emsg, 24u | 0x80000000u, "create cache subscribed emsg");
    ok &= expect_true(actions[7].reason == "7038_24", "create cache subscribed reason");
    ok &= expect_true(actions[8].type == GBE_DotaActionType::PushIncomingNow, "create ninth action pushes create ack");
    ok &= expect_eq_u64(actions[8].emsg, 7055u | 0x80000000u, "create ack emsg");
    ok &= expect_true(actions[8].reason == "7038_7055", "create ack reason");

    actions = gbe::dota_lobby_flow::create_lobby_action_list(gbe::dota_lobby_flow::CreateLobbyActionPlan{true}, false);
    ok &= expect_eq_u64(actions.size(), 10u, "create action count with previous unsubscribe");
    ok &= expect_true(actions[0].type == GBE_DotaActionType::GcMemoryReset, "create reset action before unsubscribe");
    ok &= expect_true(actions[1].type == GBE_DotaActionType::PushIncomingNow, "create unsubscribe action after reset");
    ok &= expect_eq_u64(actions[1].emsg, 25u | 0x80000000u, "create unsubscribe emsg");
    ok &= expect_true(actions[2].type == GBE_DotaActionType::GenericLobbyCreate, "create generic lobby after unsubscribe");
    ok &= expect_true(actions[7].type == GBE_DotaActionType::LobbyCacheSubscriptionRecord, "create direct cache record after publish block");
    ok &= expect_true(actions[7].reason == "7038_create_direct", "create direct cache record reason");

    return ok;
}

bool test_join_lobby_context_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::JoinLobbyContext context{};
    context.request_has_lobby_id = true;
    context.request_lobby_id = 0x704400ull;
    context.request_has_pass_key = true;
    context.request_pass_key = "context-pass";
    context.matched_generic_lobby = true;
    context.matched_lobby.active = true;
    context.matched_lobby.lobby_id = 0x704400ull;
    context.matched_lobby.generic_lobby_id = 0x7044BEEFull;
    context.matched_lobby.room_name = "join-context-room";
    context.matched_lobby.pass_key = "matched-pass";
    context.matched_lobby.owner_steam_id = 0x700ull;
    context.matched_lobby.owner_account_id = 70u;
    context.matched_lobby.owner_name = "join-owner";
    context.matched_lobby.owner_team = 0u;
    context.matched_lobby.owner_slot = 1u;
    context.matched_generic_lobby_id = 0x7044BEEFull;
    context.send_join_response = false;
    context.local_steam_id = 0x800ull;
    context.local_account_id = 80u;
    context.local_name = "join-local";
    context.good_guys_team = 0u;
    context.player_pool_team = 4u;

    const gbe::dota_lobby_state::JoinLobbyMergePlan merge_plan = gbe::dota_lobby_flow::join_lobby_merge_plan_from_context(context);
    ok &= expect_eq_u64(merge_plan.lobby.lobby_id, 0x704400ull, "join context merge lobby id");
    ok &= expect_eq_u64(merge_plan.lobby.generic_lobby_id, 0x7044BEEFull, "join context merge generic lobby id");
    ok &= expect_true(merge_plan.lobby.room_name == "join-context-room", "join context merge room name");
    ok &= expect_true(merge_plan.lobby.pass_key == "context-pass", "join context request pass key wins");
    ok &= expect_eq_u64(merge_plan.local_member.steam_id, 0x800ull, "join context local member steam id");
    ok &= expect_eq_u64(merge_plan.local_member.account_id, 80u, "join context local member account id");

    const gbe::dota_lobby_flow::JoinLobbyActionPlan action_plan = gbe::dota_lobby_flow::join_lobby_action_plan_from_context(context);
    ok &= expect_true(action_plan.matched_generic_lobby, "join context action matched generic");
    ok &= expect_true(!action_plan.send_join_response, "join context action suppresses response");
    ok &= expect_eq_u64(action_plan.generic_lobby_id, 0x7044BEEFull, "join context action generic lobby id");

    return ok;
}

bool test_join_lobby_action_list()
{
    bool ok = true;

    GBE_DotaActionList actions = gbe::dota_lobby_flow::join_lobby_action_list(gbe::dota_lobby_flow::JoinLobbyActionPlan{false, true, 0ull}, false);
    ok &= expect_eq_u64(actions.size(), 5u, "join direct action count");
    ok &= expect_true(actions[0].type == GBE_DotaActionType::LobbyLocalMemberData, "join direct first action publishes local member data");
    ok &= expect_true(actions[0].reason == "7044_join", "join direct local member reason");
    ok &= expect_true(actions[1].type == GBE_DotaActionType::LobbySnapshotRefresh, "join direct second action publishes shared lobby");
    ok &= expect_true(actions[1].reason == "7044_join", "join direct shared reason");
    ok &= expect_true(actions[2].type == GBE_DotaActionType::LobbyCacheSubscriptionRecord, "join direct third action records cache subscription");
    ok &= expect_true(actions[2].reason == "7044_join_direct", "join direct cache record reason");
    ok &= expect_true(actions[3].type == GBE_DotaActionType::PushIncomingNow, "join direct fourth action pushes cache subscribed");
    ok &= expect_eq_u64(actions[3].emsg, 24u | 0x80000000u, "join direct cache subscribed emsg");
    ok &= expect_true(actions[3].reason == "7044_join_24", "join direct cache subscribed reason");
    ok &= expect_true(actions[4].type == GBE_DotaActionType::PushIncomingNow, "join direct fifth action pushes join ack");
    ok &= expect_eq_u64(actions[4].emsg, 7113u | 0x80000000u, "join direct ack emsg");
    ok &= expect_true(actions[4].reason == "7044_join_7113", "join direct ack reason");

    actions = gbe::dota_lobby_flow::join_lobby_action_list(gbe::dota_lobby_flow::JoinLobbyActionPlan{true, true, 0x704400u}, true);
    ok &= expect_eq_u64(actions.size(), 7u, "join matched generic action count");
    ok &= expect_true(actions[0].type == GBE_DotaActionType::GenericLobbyJoin, "join matched first action joins generic lobby");
    ok &= expect_eq_u64(actions[0].item_id, 0x704400u, "join matched generic lobby id");
    ok &= expect_true(actions[0].reason == "7044_join_generic", "join matched generic join reason");
    ok &= expect_true(actions[1].type == GBE_DotaActionType::SettingsLobbySync, "join matched second action syncs settings");
    ok &= expect_true(actions[1].reason == "7044_join_generic", "join matched settings sync reason");
    ok &= expect_true(actions[4].type == GBE_DotaActionType::LobbyCacheSubscriptionRecord, "join matched records cache after publish block");
    ok &= expect_true(actions[4].reason == "7044_join_wrapped", "join matched wrapped cache record reason");
    ok &= expect_eq_u64(actions[5].emsg, 24u | 0x80000000u, "join matched cache subscribed emsg");
    ok &= expect_eq_u64(actions[6].emsg, 7113u | 0x80000000u, "join matched ack emsg");

    actions = gbe::dota_lobby_flow::join_lobby_action_list(gbe::dota_lobby_flow::JoinLobbyActionPlan{false, false, 0ull}, false);
    ok &= expect_eq_u64(actions.size(), 4u, "join without ack action count");
    ok &= expect_eq_u64(actions[3].emsg, 24u | 0x80000000u, "join without ack only pushes cache subscribed");

    return ok;
}

bool test_launch_state_plan_input_source_lobby_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::LaunchStatePushPlanInput input = valid_launch_state_plan_input();
    gbe::dota_lobby_flow::apply_source_lobby_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateSourceLobbyInput{true});

    ok &= expect_true(input.source_lobby_suppressed, "source lobby mapping suppressed");
    gbe::dota_lobby_flow::LaunchStatePushPlan plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::SuppressedSourceLobby, "source lobby mapping planner suppressed");

    gbe::dota_lobby_flow::apply_source_lobby_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateSourceLobbyInput{false});

    ok &= expect_true(!input.source_lobby_suppressed, "source lobby remapping unsuppressed");
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "source lobby remapping planner push");

    return ok;
}

bool test_launch_state_plan_input_target_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::LaunchStatePushPlanInput input{};
    gbe::dota_lobby_flow::apply_target_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateTargetInput{
            true,
            true,
            true,
            false,
            true});

    ok &= expect_true(input.source_is_server, "target mapping source server");
    ok &= expect_true(input.client_peer_available, "target mapping client peer available");
    ok &= expect_true(input.target_available, "target mapping target available");
    ok &= expect_true(!input.target_is_server, "target mapping target is client");
    ok &= expect_true(input.target_is_dota_profile, "target mapping target dota profile");

    gbe::dota_lobby_flow::LaunchStatePushPlan plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_true(plan.use_client_peer, "target mapping planner uses client peer");
    ok &= expect_true(plan.restore_shared_state, "target mapping planner restores shared state");
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::NoCapturedLobby, "target mapping planner reaches capture gate");

    gbe::dota_lobby_flow::apply_target_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateTargetInput{
            true,
            false,
            false,
            false,
            true});

    ok &= expect_true(input.source_is_server, "target remapping source server");
    ok &= expect_true(!input.client_peer_available, "target remapping client peer unavailable");
    ok &= expect_true(!input.target_available, "target remapping target unavailable");
    ok &= expect_true(!input.target_is_server, "target remapping target server flag");
    ok &= expect_true(input.target_is_dota_profile, "target remapping target dota profile");

    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::InvalidTarget, "target remapping planner invalid target");

    return ok;
}

bool test_launch_state_plan_input_shared_lobby_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::LaunchStatePushPlanInput input = valid_launch_state_plan_input();
    gbe::dota_lobby_flow::apply_shared_lobby_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateSharedLobbyInput{true});

    ok &= expect_true(input.shared_lobby_suppressed, "shared lobby mapping suppressed");
    gbe::dota_lobby_flow::LaunchStatePushPlan plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::SuppressedSharedLobby, "shared lobby mapping planner suppressed");
    ok &= expect_true(plan.restore_shared_state, "shared lobby mapping occurs after restore gate");

    gbe::dota_lobby_flow::apply_shared_lobby_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateSharedLobbyInput{false});

    ok &= expect_true(!input.shared_lobby_suppressed, "shared lobby remapping unsuppressed");
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "shared lobby remapping planner push");

    return ok;
}

bool test_launch_state_plan_input_capture_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::LaunchStatePushPlanInput input = valid_launch_state_plan_input();
    gbe::dota_lobby_flow::apply_capture_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateCaptureInput{false});

    ok &= expect_true(!input.captured_lobby_active, "capture mapping inactive");
    gbe::dota_lobby_flow::LaunchStatePushPlan plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::NoCapturedLobby, "capture mapping planner no captured lobby");

    gbe::dota_lobby_flow::apply_capture_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateCaptureInput{true});

    ok &= expect_true(input.captured_lobby_active, "capture remapping active");
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "capture remapping planner push");

    return ok;
}

bool test_launch_state_captured_lobby_input_from_local_lobby()
{
    bool ok = true;

    GBE_LocalLobby lobby{};
    lobby.active = true;
    lobby.state = 2u;
    lobby.game_state = 4u;
    lobby.server_id = 55ull;
    lobby.connect = "127.0.0.1:27015";
    lobby.owner_steam_id = 10ull;
    lobby.lan = true;
    lobby.match_id = 123ull;

    gbe::dota_lobby_flow::LaunchStateCapturedLobbyInput input = gbe::dota_lobby_flow::captured_lobby_input_from_local_lobby(lobby, true);
    ok &= expect_true(input.active, "local lobby capture input active");
    ok &= expect_eq_u64(input.state, 2u, "local lobby capture input state");
    ok &= expect_eq_u64(input.game_state, 4u, "local lobby capture input game state");
    ok &= expect_eq_u64(input.server_id, 55ull, "local lobby capture input server id");
    ok &= expect_true(input.connect_available, "local lobby capture input connect available");
    ok &= expect_eq_u64(input.owner_steam_id, 10ull, "local lobby capture input owner steam id");
    ok &= expect_true(input.lan, "local lobby capture input lan");
    ok &= expect_eq_u64(input.match_id, 123ull, "local lobby capture input match id");

    lobby.connect.clear();
    input = gbe::dota_lobby_flow::captured_lobby_input_from_local_lobby(lobby, false);
    ok &= expect_true(!input.active, "local lobby capture input inactive");
    ok &= expect_true(!input.connect_available, "local lobby capture input missing connect");

    return ok;
}

bool test_launch_state_plan_input_captured_lobby_mapping()
{
    bool ok = true;

    gbe::dota_lobby_flow::LaunchStatePushPlanInput input{};
    input.source_is_server = true;
    input.client_peer_available = true;
    input.target_available = true;
    input.target_is_server = false;
    input.target_is_dota_profile = true;
    input.shared_lobby_suppressed = false;

    gbe::dota_lobby_flow::apply_captured_lobby_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateCapturedLobbyInput{
            true,
            2u,
            4u,
            0ull,
            true,
            10ull,
            true,
            123ull},
        gbe::dota_lobby_flow::LaunchStateCapturedContextInput{3u, 10ull});

    ok &= expect_true(input.captured_lobby_active, "captured lobby mapping active");
    ok &= expect_eq_u64(input.lobby_state, 2u, "captured lobby mapping state");
    ok &= expect_eq_u64(input.lobby_game_state, 4u, "captured lobby mapping game state");
    ok &= expect_eq_u64(input.lobby_server_id, 0ull, "captured lobby mapping server id");
    ok &= expect_true(input.lobby_connect_available, "captured lobby mapping connect availability");
    ok &= expect_eq_u64(input.last_pushed_game_state, 3u, "captured lobby mapping last pushed game state");
    ok &= expect_eq_u64(input.target_local_steam_id, 10ull, "captured lobby mapping target local steam id");
    ok &= expect_eq_u64(input.lobby_owner_steam_id, 10ull, "captured lobby mapping owner steam id");
    ok &= expect_true(input.lobby_lan, "captured lobby mapping lan flag");
    ok &= expect_eq_u64(input.lobby_match_id, 123ull, "captured lobby mapping match id");

    gbe::dota_lobby_flow::LaunchStatePushPlan plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::None, "captured lobby mapping planner push");
    ok &= expect_true(plan.preserve_server_id, "captured lobby mapping planner preserve owner lan server id");

    gbe::dota_lobby_flow::apply_captured_lobby_to_launch_state_push_plan_input(
        input,
        gbe::dota_lobby_flow::LaunchStateCapturedLobbyInput{
            true,
            2u,
            4u,
            0ull,
            true,
            10ull,
            true,
            123ull},
        gbe::dota_lobby_flow::LaunchStateCapturedContextInput{4u, 20ull});

    ok &= expect_eq_u64(input.last_pushed_game_state, 4u, "captured context remapping last pushed game state");
    ok &= expect_eq_u64(input.target_local_steam_id, 20ull, "captured context remapping target local steam id");
    plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    ok &= expect_eq_skip_reason(plan.skip_reason, gbe::dota_lobby_flow::LaunchStatePushSkipReason::DuplicateGameState, "captured context remapping planner duplicate");
    ok &= expect_true(!plan.preserve_server_id, "captured context remapping skips preserve for non-owner target");

    return ok;
}

bool test_upsert_and_slot_selection()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u)};

    gbe::dota_lobby_flow::upsert_lobby_member(members, member(0ull, 9u));
    ok &= expect_eq_u64(members.size(), 1u, "ignore zero upsert");

    gbe::dota_lobby_flow::upsert_lobby_member(members, member(20ull, 2u));
    ok &= expect_eq_u64(members.size(), 2u, "insert member");

    GBE_DotaLobbyMemberState updated = member(20ull, 5u);
    gbe::dota_lobby_flow::upsert_lobby_member(members, updated);
    ok &= expect_eq_u64(members.size(), 2u, "update member keeps size");
    ok &= expect_eq_u64(members[1].slot, 5u, "update member slot");

    ok &= expect_eq_u64(gbe::dota_lobby_flow::select_arcade_lobby_member_slot(members, 20ull, 1u), 5u, "reuse existing slot");
    ok &= expect_eq_u64(gbe::dota_lobby_flow::select_arcade_lobby_member_slot(members, 30ull, 1u), 2u, "select first free slot");

    return ok;
}

bool test_apply_lobby_member_team_slot_update()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u)};
    members[0].team = kPlayerPool;

    gbe::dota_lobby_flow::apply_lobby_member_team_slot_update(members, 10ull, 10u, true, kGoodGuys, true, 4u, kPlayerPool, true);
    ok &= expect_eq_u64(members.size(), 1u, "team slot update keeps existing size");
    ok &= expect_eq_u64(members[0].team, kGoodGuys, "team slot update team");
    ok &= expect_eq_u64(members[0].slot, 4u, "team slot update slot");

    gbe::dota_lobby_flow::apply_lobby_member_team_slot_update(members, 20ull, 200u, false, 0u, false, 0u, kPlayerPool, false);
    ok &= expect_eq_u64(members.size(), 2u, "team slot update inserts member");
    ok &= expect_eq_u64(members[1].steam_id, 20ull, "team slot insert steam id");
    ok &= expect_eq_u64(members[1].account_id, 200u, "team slot insert account id");
    ok &= expect_eq_u64(members[1].team, kPlayerPool, "team slot insert default team");
    ok &= expect_eq_u64(members[1].slot, 0u, "team slot insert default slot");
    ok &= expect_true(!members[1].connected, "team slot insert connected flag");

    gbe::dota_lobby_flow::apply_lobby_member_team_slot_update(members, 30ull, 300u, true, kGoodGuys, true, 6u, kPlayerPool, true);
    ok &= expect_eq_u64(members.size(), 3u, "team slot update inserts explicit member");
    ok &= expect_eq_u64(members[2].team, kGoodGuys, "team slot insert explicit team");
    ok &= expect_eq_u64(members[2].slot, 6u, "team slot insert explicit slot");
    ok &= expect_true(members[2].connected, "team slot insert explicit connected");

    gbe::dota_lobby_flow::apply_lobby_member_team_slot_update(members, 0ull, 0u, true, 9u, true, 9u, kPlayerPool, true);
    ok &= expect_eq_u64(members.size(), 3u, "team slot update ignores zero steam id");

    return ok;
}

bool test_member_state_update_block()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u), member(20ull, 0u)};
    members[1].team = kPlayerPool;
    members[1].connected = false;

    ok &= expect_true(gbe::dota_lobby_flow::set_lobby_member_connected(members, 20ull, 20u, true, true, false, 10ull, 1u, kGoodGuys, kPlayerPool), "connect existing member changes");
    ok &= expect_true(members[1].connected, "connect existing member flag");
    ok &= expect_eq_u64(members[1].team, kGoodGuys, "connect normalizes team");
    ok &= expect_eq_u64(members[1].slot, 2u, "connect normalizes slot");

    ok &= expect_true(gbe::dota_lobby_flow::set_lobby_member_connected(members, 20ull, 20u, false, true, true, 10ull, 1u, kGoodGuys, kPlayerPool), "disconnect marks existing member");
    ok &= expect_true(!members[1].connected, "disconnect existing member flag");
    ok &= expect_eq_u64(members[1].leaver_status, 1u, "disconnect marks leaver");

    ok &= expect_true(gbe::dota_lobby_flow::set_lobby_member_connected(members, 20ull, 20u, true, true, false, 10ull, 1u, kGoodGuys, kPlayerPool), "reconnect clears leaver");
    ok &= expect_eq_u64(members[1].leaver_status, 0u, "reconnect leaver status");

    ok &= expect_true(gbe::dota_lobby_flow::set_lobby_member_connected(members, 30ull, 300u, true, true, false, 10ull, 1u, kGoodGuys, kPlayerPool), "connect inserts remote member");
    ok &= expect_eq_u64(members.size(), 3u, "connect insert size");
    ok &= expect_eq_u64(members[2].steam_id, 30ull, "connect insert steam id");
    ok &= expect_eq_u64(members[2].account_id, 300u, "connect insert account id");
    ok &= expect_eq_u64(members[2].team, kGoodGuys, "connect insert normalized team");
    ok &= expect_eq_u64(members[2].slot, 3u, "connect insert normalized slot");

    ok &= expect_true(!gbe::dota_lobby_flow::set_lobby_member_connected(members, 10ull, 10u, true, true, false, 10ull, 1u, kGoodGuys, kPlayerPool), "connect does not insert owner");
    ok &= expect_eq_u64(members.size(), 3u, "connect owner keeps size");

    ok &= expect_true(gbe::dota_lobby_flow::set_lobby_member_hero(members, 20ull, 88u), "hero update changes member");
    ok &= expect_eq_u64(members[1].hero_id, 88u, "hero update value");
    ok &= expect_true(!gbe::dota_lobby_flow::set_lobby_member_hero(members, 20ull, 88u), "hero update unchanged");
    ok &= expect_true(!gbe::dota_lobby_flow::set_lobby_member_hero(members, 20ull, 0u), "hero update ignores zero hero");
    ok &= expect_true(!gbe::dota_lobby_flow::set_lobby_member_hero(members, 40ull, 90u), "hero update missing member");

    return ok;
}

bool test_owner_adoption_block()
{
    bool ok = true;

    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u), member(20ull, 4u)};
    members[1].account_id = 222u;
    members[1].team = 3u;
    members[1].hero_id = 99u;
    members[1].connected = false;
    std::uint64_t owner_steam_id = 10ull;
    std::uint32_t owner_account_id = 10u;
    std::uint32_t owner_team = kGoodGuys;
    std::uint32_t owner_slot = 1u;
    std::uint32_t owner_hero_id = 11u;
    bool owner_connected = true;

    GBE_DotaLobbyMemberState adopted = gbe::dota_lobby_flow::adopt_lobby_owner_member(
        members,
        20ull,
        200u,
        kGoodGuys,
        true,
        owner_steam_id,
        owner_account_id,
        owner_team,
        owner_slot,
        owner_hero_id,
        owner_connected);
    ok &= expect_eq_u64(adopted.account_id, 222u, "adopt existing owner account");
    ok &= expect_eq_u64(owner_steam_id, 20ull, "adopt existing owner steam id");
    ok &= expect_eq_u64(owner_account_id, 222u, "adopt existing owner account field");
    ok &= expect_eq_u64(owner_team, 3u, "adopt existing owner team");
    ok &= expect_eq_u64(owner_slot, 4u, "adopt existing owner slot");
    ok &= expect_eq_u64(owner_hero_id, 99u, "adopt existing owner hero");
    ok &= expect_true(!owner_connected, "adopt existing owner connected");
    ok &= expect_eq_u64(members.size(), 2u, "adopt existing keeps member count");

    adopted = gbe::dota_lobby_flow::adopt_lobby_owner_member(
        members,
        30ull,
        300u,
        kGoodGuys,
        true,
        owner_steam_id,
        owner_account_id,
        owner_team,
        owner_slot,
        owner_hero_id,
        owner_connected);
    ok &= expect_eq_u64(adopted.steam_id, 30ull, "adopt missing owner steam id");
    ok &= expect_eq_u64(adopted.account_id, 300u, "adopt missing owner account");
    ok &= expect_eq_u64(adopted.team, kGoodGuys, "adopt missing owner team");
    ok &= expect_eq_u64(adopted.slot, 0u, "adopt missing owner slot");
    ok &= expect_true(adopted.connected, "adopt missing owner connected");
    ok &= expect_eq_u64(owner_steam_id, 30ull, "adopt missing owner steam field");
    ok &= expect_eq_u64(owner_account_id, 300u, "adopt missing owner account field");
    ok &= expect_eq_u64(members.size(), 3u, "adopt missing inserts member");
    ok &= expect_eq_u64(members[2].steam_id, 30ull, "adopt missing inserted steam id");

    return ok;
}

bool test_generic_snapshot_block()
{
    bool ok = true;

    std::vector<GBE_DotaLobbyMemberState> existing{member(10ull, 1u), member(20ull, 2u), member(30ull, 3u)};
    existing[1].connected = true;
    existing[2].connected = true;
    GBE_DotaLobbyMemberState generic_member = member(20ull, 2u);
    generic_member.connected = false;
    gbe::dota_lobby_flow::preserve_generic_snapshot_member_runtime(generic_member, existing, true, false);
    ok &= expect_true(generic_member.connected, "snapshot preserves launched lan connected member");

    generic_member.connected = false;
    generic_member.leaver_status = 0u;
    gbe::dota_lobby_flow::preserve_generic_snapshot_member_runtime(generic_member, existing, true, true);
    ok &= expect_true(!generic_member.connected, "snapshot custom runtime keeps disconnected");
    ok &= expect_eq_u64(generic_member.leaver_status, 1u, "snapshot custom runtime marks leaver");

    std::vector<GBE_DotaLobbyMemberState> snapshot_members{member(20ull, 2u)};
    gbe::dota_lobby_flow::merge_existing_lobby_members_for_generic_snapshot(snapshot_members, existing, false, 10ull, 10ull, true, true);
    ok &= expect_eq_u64(snapshot_members.size(), 3u, "snapshot merge preserves local and missing members");
    ok &= expect_eq_u64(snapshot_members[1].steam_id, 10ull, "snapshot merge preserves local member");
    ok &= expect_eq_u64(snapshot_members[2].steam_id, 30ull, "snapshot merge preserves missing member");
    ok &= expect_true(!snapshot_members[2].connected, "snapshot merge marks missing disconnected");
    ok &= expect_eq_u64(snapshot_members[2].leaver_status, 1u, "snapshot merge marks missing leaver");

    std::vector<GBE_DotaLobbyMemberState> empty_snapshot;
    gbe::dota_lobby_flow::merge_existing_lobby_members_for_generic_snapshot(empty_snapshot, existing, true, 10ull, 10ull, false, false);
    ok &= expect_eq_u64(empty_snapshot.size(), 3u, "snapshot merge keeps all when generic empty");

    gbe::dota_lobby_flow::upsert_owner_member_for_generic_snapshot(snapshot_members, false, false, 40ull, 400u, kGoodGuys, 1u, 70u, true);
    ok &= expect_eq_u64(snapshot_members.size(), 3u, "snapshot owner skipped when absent from non-empty generic");
    gbe::dota_lobby_flow::upsert_owner_member_for_generic_snapshot(snapshot_members, true, false, 40ull, 400u, kGoodGuys, 1u, 70u, true);
    ok &= expect_eq_u64(snapshot_members.size(), 4u, "snapshot owner inserted when present in generic");
    ok &= expect_eq_u64(snapshot_members[3].steam_id, 40ull, "snapshot owner inserted steam id");
    gbe::dota_lobby_flow::upsert_owner_member_for_generic_snapshot(snapshot_members, false, true, 50ull, 500u, kGoodGuys, 2u, 80u, false);
    ok &= expect_eq_u64(snapshot_members.size(), 5u, "snapshot owner inserted when generic empty");
    ok &= expect_eq_u64(snapshot_members[4].steam_id, 50ull, "snapshot owner empty generic steam id");
    gbe::dota_lobby_flow::upsert_owner_member_for_generic_snapshot(snapshot_members, true, false, 0ull, 0u, kGoodGuys, 0u, 0u, false);
    ok &= expect_eq_u64(snapshot_members.size(), 5u, "snapshot owner ignores zero steam id");

    return ok;
}

bool test_chat_channel_sync_block()
{
    bool ok = true;

    ok &= expect_true(gbe::dota_lobby_flow::resolve_chat_member_display_name(10ull, 10ull, "Local", 20ull, "Owner", "Generic", "Friend", "Fallback") == "Local", "chat name local priority");
    ok &= expect_true(gbe::dota_lobby_flow::resolve_chat_member_display_name(20ull, 10ull, "Local", 20ull, "Owner", "Generic", "Friend", "Fallback") == "Owner", "chat name owner priority");
    ok &= expect_true(gbe::dota_lobby_flow::resolve_chat_member_display_name(30ull, 10ull, "Local", 20ull, "Owner", "Generic", "Friend", "Fallback") == "Generic", "chat name generic priority");
    ok &= expect_true(gbe::dota_lobby_flow::resolve_chat_member_display_name(30ull, 10ull, "Local", 20ull, "Owner", "", "Friend", "Fallback") == "Friend", "chat name friend priority");
    ok &= expect_true(gbe::dota_lobby_flow::resolve_chat_member_display_name(30ull, 10ull, "Local", 20ull, "Owner", "", "Unknown User", "Fallback") == "Fallback", "chat name fallback priority");
    ok &= expect_true(gbe::dota_lobby_flow::resolve_chat_member_display_name(30ull, 10ull, "Local", 20ull, "Owner", "", "Unknown User", "") == "30", "chat name steam id fallback");

    std::vector<GBE_DotaLobbyMemberState> channel_members{member(10ull, 1u), member(20ull, 2u), member(30ull, 3u)};
    std::vector<GBE_DotaChatMemberState> resolved_names{{30ull, "Remote"}};
    std::vector<GBE_DotaChatMemberState> chat_members = gbe::dota_lobby_flow::compose_join_chat_channel_members(10ull, "Local", channel_members, 20ull, "Owner", resolved_names);
    ok &= expect_eq_u64(chat_members.size(), 4u, "chat members preserve local duplicate behavior");
    ok &= expect_eq_u64(chat_members[0].steam_id, 10ull, "chat first local from channel");
    ok &= expect_true(chat_members[0].name == "Local", "chat first local name");
    ok &= expect_eq_u64(chat_members[1].steam_id, 10ull, "chat second local explicit");
    ok &= expect_eq_u64(chat_members[2].steam_id, 20ull, "chat owner member id");
    ok &= expect_true(chat_members[2].name == "Owner", "chat owner member name");
    ok &= expect_eq_u64(chat_members[3].steam_id, 30ull, "chat remote member id");
    ok &= expect_true(chat_members[3].name == "Remote", "chat remote member name");

    std::vector<GBE_DotaChatMemberState> chat_without_local = gbe::dota_lobby_flow::compose_join_chat_channel_members(10ull, "Local", std::vector<GBE_DotaLobbyMemberState>{member(30ull, 3u)}, 20ull, "Owner", resolved_names);
    ok &= expect_eq_u64(chat_without_local.size(), 2u, "chat members add explicit local when absent");
    ok &= expect_eq_u64(chat_without_local[0].steam_id, 10ull, "chat absent local explicit id");
    ok &= expect_eq_u64(chat_without_local[1].steam_id, 30ull, "chat absent local remote id");

    return ok;
}

bool test_details_update_payload_routing_block()
{
    bool ok = true;

    ok &= expect_true(
        gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_details_update(0ull, false, 2u, 0ull),
        "details update uses current payload for prelaunch multi member lobby");
    ok &= expect_true(
        !gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_details_update(0ull, false, 1u, 0ull),
        "details update uses template replay for prelaunch single member lobby");
    ok &= expect_true(
        gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_details_update(100ull, true, 2u, 0ull),
        "details update uses current payload for launched lan multi member lobby");
    ok &= expect_true(
        !gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_details_update(100ull, false, 2u, 0ull),
        "details update uses template replay for launched non-lan lobby");
    ok &= expect_true(
        !gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_details_update(100ull, true, 1u, 0ull),
        "details update uses template replay for launched lan single member lobby");
    ok &= expect_true(
        gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_details_update(100ull, false, 1u, 55ull),
        "details update uses current payload for custom game lobby");

    return ok;
}

bool test_cache_subscribed_payload_routing_block()
{
    bool ok = true;

    ok &= expect_true(
        gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_cache_subscribed(false, false, 2u),
        "cache subscribed uses current payload for prelaunch multi member lobby");
    ok &= expect_true(
        !gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_cache_subscribed(false, false, 1u),
        "cache subscribed uses template replay for prelaunch single member lobby");
    ok &= expect_true(
        gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_cache_subscribed(true, true, 2u),
        "cache subscribed uses current payload for launched lan multi member lobby");
    ok &= expect_true(
        !gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_cache_subscribed(true, false, 2u),
        "cache subscribed uses launch template for launched non-lan lobby");
    ok &= expect_true(
        !gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_cache_subscribed(true, true, 1u),
        "cache subscribed uses launch template for launched lan single member lobby");

    return ok;
}

bool test_authoritative_lobby_payload_data_block()
{
    bool ok = true;

    GBE_DotaAuthoritativeLobbyPayloadData data = gbe::dota_lobby_flow::compose_authoritative_lobby_payload_data(
        false,
        true,
        100ull,
        200ull,
        "connect");
    ok &= expect_eq_u64(data.server_id, 0ull, "authoritative payload clears server when preserve disabled");
    ok &= expect_true(data.connect == "connect", "authoritative payload keeps connect");

    data = gbe::dota_lobby_flow::compose_authoritative_lobby_payload_data(
        true,
        false,
        100ull,
        200ull,
        "connect");
    ok &= expect_eq_u64(data.server_id, 0ull, "authoritative payload clears server before launch");

    data = gbe::dota_lobby_flow::compose_authoritative_lobby_payload_data(
        true,
        true,
        100ull,
        200ull,
        "connect");
    ok &= expect_eq_u64(data.server_id, 100ull, "authoritative payload preserves current server id");

    data = gbe::dota_lobby_flow::compose_authoritative_lobby_payload_data(
        true,
        true,
        0ull,
        200ull,
        "connect");
    ok &= expect_eq_u64(data.server_id, 200ull, "authoritative payload uses server candidate fallback");

    return ok;
}

bool test_lobby_publish_data_block()
{
    bool ok = true;

    GBE_DotaLobbyMemberState local = member(10ull, 2u);
    local.team = kPlayerPool;
    local.hero_id = 88u;
    local.connected = false;
    GBE_DotaLobbyMemberPublishData member_data = gbe::dota_lobby_flow::compose_lobby_member_publish_data(local, "Local");
    ok &= expect_eq_u64(member_data.team, kPlayerPool, "publish member team");
    ok &= expect_eq_u64(member_data.slot, 2u, "publish member slot");
    ok &= expect_eq_u64(member_data.hero_id, 88u, "publish member hero");
    ok &= expect_true(!member_data.connected, "publish member connected");
    ok &= expect_true(member_data.name == "Local", "publish member name");

    GBE_DotaLobbyMetadataPublishData metadata = gbe::dota_lobby_flow::compose_lobby_metadata_publish_data(
        "",
        "",
        "Fallback Owner",
        0ull,
        123ull,
        "127.0.0.1:27015");
    ok &= expect_true(metadata.room_name == "Lobby", "metadata room fallback");
    ok &= expect_true(metadata.owner_name == "Fallback Owner", "metadata owner fallback");
    ok &= expect_eq_u64(metadata.server_id, 0ull, "metadata clears prelaunch server id");
    ok &= expect_true(metadata.connect == "127.0.0.1:27015", "metadata connect value");
    ok &= expect_true(gbe::dota_lobby_flow::should_clear_lobby_server_id_for_metadata_publish(0ull), "metadata clear prelaunch server id predicate");

    metadata = gbe::dota_lobby_flow::compose_lobby_metadata_publish_data(
        "Room",
        "Owner",
        "Fallback Owner",
        55ull,
        123ull,
        "10.0.0.1:27015");
    ok &= expect_true(metadata.room_name == "Room", "metadata room explicit");
    ok &= expect_true(metadata.owner_name == "Owner", "metadata owner explicit");
    ok &= expect_eq_u64(metadata.server_id, 123ull, "metadata preserves launched server id");
    ok &= expect_true(!gbe::dota_lobby_flow::should_clear_lobby_server_id_for_metadata_publish(55ull), "metadata preserve launched server id predicate");

    GBE_DotaLobbyOptionsPublishData options = gbe::dota_lobby_flow::compose_lobby_options_publish_data(
        7u,
        8u,
        "lan_ping",
        "pass123",
        true,
        false,
        true,
        2u,
        3u,
        4u,
        5ull,
        6ull);
    ok &= expect_true(options.game_mode == "7", "options game mode string");
    ok &= expect_true(options.server_region == "8", "options server region string");
    ok &= expect_true(options.lan_host_ping_location == "lan_ping", "options lan ping string");
    ok &= expect_true(options.pass_key == "pass123", "options pass key string");
    ok &= expect_true(options.allow_cheats == "1", "options allow cheats string");
    ok &= expect_true(options.fill_with_bots == "0", "options fill with bots string");
    ok &= expect_true(options.allow_spectating == "1", "options allow spectating string");
    ok &= expect_true(options.visibility == "2", "options visibility string");
    ok &= expect_true(options.bot_difficulty_radiant == "3", "options radiant difficulty string");
    ok &= expect_true(options.bot_difficulty_dire == "4", "options dire difficulty string");
    ok &= expect_true(options.bot_radiant == "5", "options radiant bot string");
    ok &= expect_true(options.bot_dire == "6", "options dire bot string");

    GBE_DotaLobbyScalarPublishData scalar = gbe::dota_lobby_flow::compose_lobby_scalar_publish_data(
        11ull,
        22ull,
        33ull,
        44u,
        55u,
        66ull,
        77u,
        88u,
        99u);
    ok &= expect_true(scalar.dota_lobby_id == "11", "scalar lobby id string");
    ok &= expect_true(scalar.owner_steam_id == "22", "scalar owner steam id string");
    ok &= expect_true(scalar.owner_account_id == "33", "scalar owner account id string");
    ok &= expect_true(scalar.state == "44", "scalar state string");
    ok &= expect_true(scalar.game_state == "55", "scalar game state string");
    ok &= expect_true(scalar.match_id == "66", "scalar match id string");
    ok &= expect_true(scalar.game_start_time == "77", "scalar game start time string");
    ok &= expect_true(scalar.tv_secret_code == "88", "scalar tv secret code string");
    ok &= expect_true(scalar.tv_port == "99", "scalar tv port string");

    GBE_DotaGenericLobbySnapshotScalarData snapshot_scalar = gbe::dota_lobby_flow::compose_generic_lobby_snapshot_scalar_data(
        "",
        12u,
        13u,
        0u,
        14u,
        15ull,
        16ull,
        "connect",
        17u,
        "lan_ping",
        "pass",
        true,
        false,
        true,
        18u,
        19u,
        20u,
        21ull,
        22ull,
        23ull,
        24u);
    ok &= expect_true(snapshot_scalar.room_name == "Lobby", "snapshot room fallback string");
    ok &= expect_eq_u64(snapshot_scalar.state, 1u, "snapshot state normalized to one");
    ok &= expect_true(snapshot_scalar.lan, "snapshot lan flag");
    ok &= expect_true(snapshot_scalar.connect == "connect", "snapshot connect string");
    ok &= expect_eq_u64(snapshot_scalar.tv_port, 24u, "snapshot tv port value");

    GBE_DotaGenericLobbyMemberSnapshotInput member_input{};
    member_input.member_steam_id = 42ull;
    member_input.member_account_id = 420u;
    member_input.owner_steam_id = 42ull;
    member_input.owner_account_id = 421u;
    member_input.owner_team = kGoodGuys;
    member_input.owner_slot = 1u;
    member_input.owner_hero_id = 88u;
    member_input.owner_connected = false;
    member_input.member_team_raw = "3";
    member_input.member_slot_raw = "5";
    member_input.member_hero_raw = "9";
    member_input.member_connected_raw = "1";
    member_input.player_pool_team = kPlayerPool;
    GBE_DotaLobbyMemberSnapshotData member_snapshot = gbe::dota_lobby_flow::compose_generic_lobby_member_snapshot_data(member_input);
    ok &= expect_eq_u64(member_snapshot.member.team, 3u, "generic member parsed team");
    ok &= expect_eq_u64(member_snapshot.member.slot, 5u, "generic member parsed slot");
    ok &= expect_eq_u64(member_snapshot.member.hero_id, 9u, "generic member parsed hero");
    ok &= expect_true(member_snapshot.member.connected, "generic member parsed connected");
    ok &= expect_eq_u64(member_snapshot.owner_team, 3u, "generic member owner team sync");
    ok &= expect_eq_u64(member_snapshot.owner_slot, 5u, "generic member owner slot sync");

    member_input.member_steam_id = 43ull;
    member_input.owner_steam_id = 42ull;
    member_input.member_team_raw.clear();
    member_input.member_slot_raw.clear();
    member_input.member_hero_raw = "10";
    member_input.member_connected_raw = "0";
    member_snapshot = gbe::dota_lobby_flow::compose_generic_lobby_member_snapshot_data(member_input);
    ok &= expect_eq_u64(member_snapshot.member.team, kPlayerPool, "generic member defaults to player pool");
    ok &= expect_eq_u64(member_snapshot.member.slot, 0u, "generic member defaults slot to zero");
    ok &= expect_true(!member_snapshot.member.connected, "generic member parsed disconnected");

    member_input.member_team_raw = "bad";
    member_input.member_slot_raw = "4294967296";
    member_input.member_hero_raw.clear();
    member_input.member_connected_raw = "yes";
    member_snapshot = gbe::dota_lobby_flow::compose_generic_lobby_member_snapshot_data(member_input);
    ok &= expect_eq_u64(member_snapshot.member.team, 0u, "generic member bad team parses zero");
    ok &= expect_eq_u64(member_snapshot.member.slot, 0u, "generic member overflow slot parses zero");
    ok &= expect_eq_u64(member_snapshot.member.hero_id, 0u, "generic member empty hero parses zero");
    ok &= expect_true(!member_snapshot.member.connected, "generic member bad connected parses false");

    std::vector<GBE_DotaLobbyMemberState> sync_members;
    std::uint32_t owner_team = kGoodGuys;
    std::uint32_t owner_slot = 1u;
    std::uint32_t owner_hero_id = 88u;
    bool owner_connected = false;
    GBE_DotaLobbyMemberSnapshotData owner_snapshot{};
    owner_snapshot.member = member(42ull, 2u);
    owner_snapshot.member.team = 3u;
    owner_snapshot.member.hero_id = 77u;
    owner_snapshot.owner_team = 3u;
    owner_snapshot.owner_slot = 2u;
    owner_snapshot.owner_hero_id = 77u;
    owner_snapshot.owner_connected = true;
    gbe::dota_lobby_flow::apply_generic_lobby_member_snapshot(
        sync_members,
        owner_snapshot,
        true,
        42ull,
        owner_team,
        owner_slot,
        owner_hero_id,
        owner_connected,
        kGoodGuys,
        kPlayerPool);
    ok &= expect_eq_u64(sync_members.size(), 1u, "member sync inserts owner");
    ok &= expect_eq_u64(owner_team, 3u, "member sync owner team");
    ok &= expect_eq_u64(owner_slot, 2u, "member sync owner slot");
    ok &= expect_eq_u64(owner_hero_id, 77u, "member sync owner hero");
    ok &= expect_true(owner_connected, "member sync owner connected");

    GBE_DotaLobbyMemberSnapshotData remote_snapshot{};
    remote_snapshot.member = member(43ull, 0u);
    remote_snapshot.member.team = kPlayerPool;
    remote_snapshot.owner_team = owner_team;
    remote_snapshot.owner_slot = owner_slot;
    remote_snapshot.owner_hero_id = owner_hero_id;
    remote_snapshot.owner_connected = owner_connected;
    gbe::dota_lobby_flow::apply_generic_lobby_member_snapshot(
        sync_members,
        remote_snapshot,
        true,
        42ull,
        owner_team,
        owner_slot,
        owner_hero_id,
        owner_connected,
        kGoodGuys,
        kPlayerPool);
    ok &= expect_eq_u64(sync_members.size(), 2u, "member sync inserts remote");
    ok &= expect_eq_u64(sync_members[1].team, kGoodGuys, "member sync normalizes remote team");
    ok &= expect_eq_u64(sync_members[1].slot, 1u, "member sync normalizes remote slot");

    return ok;
}

bool test_generic_lobby_snapshot_basic_block()
{
    bool ok = true;

    GBE_DotaLobbyOwnerSnapshotData owner = gbe::dota_lobby_flow::compose_lobby_owner_snapshot_data(
        10ull,
        10u,
        "Stored Owner",
        20ull,
        20u,
        true,
        30ull,
        "Local",
        99u,
        "Lobby Host");
    ok &= expect_eq_u64(owner.owner_steam_id, 20ull, "snapshot generic owner steam overrides stored owner");
    ok &= expect_eq_u64(owner.owner_account_id, 20u, "snapshot generic owner account overrides stored owner");
    ok &= expect_true(owner.owner_name == "Stored Owner", "snapshot keeps stored remote owner name");
    ok &= expect_true(!owner.should_publish_local_owner, "snapshot remote owner does not publish local owner");

    owner = gbe::dota_lobby_flow::compose_lobby_owner_snapshot_data(
        10ull,
        0u,
        "Stored Owner",
        30ull,
        30u,
        true,
        30ull,
        "Local",
        99u,
        "Lobby Host");
    ok &= expect_eq_u64(owner.owner_steam_id, 30ull, "snapshot local generic owner steam");
    ok &= expect_eq_u64(owner.owner_account_id, 30u, "snapshot local generic owner account");
    ok &= expect_true(owner.owner_name == "Local", "snapshot local owner name");
    ok &= expect_true(owner.should_publish_local_owner, "snapshot marks local owner publish");

    owner = gbe::dota_lobby_flow::compose_lobby_owner_snapshot_data(
        40ull,
        0u,
        "",
        0ull,
        0u,
        false,
        30ull,
        "Local",
        99u,
        "Lobby Host");
    ok &= expect_eq_u64(owner.owner_steam_id, 40ull, "snapshot keeps stored owner when generic invalid");
    ok &= expect_eq_u64(owner.owner_account_id, 99u, "snapshot fallback account id");
    ok &= expect_true(owner.owner_name == "Lobby Host", "snapshot fallback owner name");

    owner = gbe::dota_lobby_flow::compose_lobby_owner_snapshot_data(
        40ull,
        400u,
        "Stored Owner",
        0ull,
        0u,
        false,
        30ull,
        "Local",
        99u,
        "Lobby Host");
    ok &= expect_eq_u64(owner.owner_account_id, 400u, "snapshot keeps stored owner account id");
    ok &= expect_true(owner.owner_name == "Stored Owner", "snapshot keeps stored owner name without generic owner");

    GBE_DotaLobbyOwnerPublishData owner_publish = gbe::dota_lobby_flow::compose_lobby_owner_publish_data(owner, "Local");
    ok &= expect_eq_u64(owner_publish.owner_steam_id, 40ull, "owner publish steam id");
    ok &= expect_eq_u64(owner_publish.owner_account_id, 400u, "owner publish account id");
    ok &= expect_true(owner_publish.owner_name == "Stored Owner", "owner publish keeps remote owner name");
    ok &= expect_true(!owner_publish.should_publish_local_owner, "owner publish remote flag");

    owner.should_publish_local_owner = true;
    owner_publish = gbe::dota_lobby_flow::compose_lobby_owner_publish_data(owner, "Local");
    ok &= expect_true(owner_publish.owner_name == "Local", "owner publish switches to local name");
    ok &= expect_true(owner_publish.should_publish_local_owner, "owner publish local flag");

    ok &= expect_eq_u64(gbe::dota_lobby_flow::resolve_snapshot_lobby_state(0u), 1u, "snapshot state zero defaults to one");
    ok &= expect_eq_u64(gbe::dota_lobby_flow::resolve_snapshot_lobby_state(3u), 3u, "snapshot state keeps nonzero");
    ok &= expect_true(gbe::dota_lobby_flow::resolve_snapshot_room_name("") == "Lobby", "snapshot room fallback");
    ok &= expect_true(gbe::dota_lobby_flow::resolve_snapshot_room_name("Room") == "Room", "snapshot room explicit");

    return ok;
}

bool test_generic_lobby_snapshot_member_parse_block()
{
    bool ok = true;

    GBE_DotaLobbyMemberSnapshotData owner = gbe::dota_lobby_flow::compose_lobby_member_snapshot_data(
        10ull,
        10u,
        10ull,
        100u,
        kGoodGuys,
        1u,
        11u,
        false,
        true,
        3u,
        true,
        5u,
        99u,
        true,
        kPlayerPool);
    ok &= expect_eq_u64(owner.member.steam_id, 10ull, "snapshot owner member steam id");
    ok &= expect_eq_u64(owner.member.account_id, 100u, "snapshot owner member account override");
    ok &= expect_eq_u64(owner.member.team, 3u, "snapshot owner member parsed team");
    ok &= expect_eq_u64(owner.member.slot, 5u, "snapshot owner member parsed slot");
    ok &= expect_eq_u64(owner.member.hero_id, 99u, "snapshot owner member hero");
    ok &= expect_true(owner.member.connected, "snapshot owner member connected");
    ok &= expect_eq_u64(owner.owner_team, 3u, "snapshot owner team sync");
    ok &= expect_eq_u64(owner.owner_slot, 5u, "snapshot owner slot sync");
    ok &= expect_eq_u64(owner.owner_hero_id, 99u, "snapshot owner hero sync");
    ok &= expect_true(owner.owner_connected, "snapshot owner connected sync");

    owner = gbe::dota_lobby_flow::compose_lobby_member_snapshot_data(
        10ull,
        10u,
        10ull,
        100u,
        kGoodGuys,
        1u,
        11u,
        false,
        false,
        0u,
        false,
        0u,
        77u,
        false,
        kPlayerPool);
    ok &= expect_eq_u64(owner.member.team, kGoodGuys, "snapshot owner keeps default team without raw");
    ok &= expect_eq_u64(owner.member.slot, 1u, "snapshot owner keeps default slot without raw");
    ok &= expect_eq_u64(owner.owner_team, kGoodGuys, "snapshot owner team remains default");
    ok &= expect_eq_u64(owner.owner_slot, 1u, "snapshot owner slot remains default");
    ok &= expect_eq_u64(owner.owner_hero_id, 77u, "snapshot owner hero sync without team slot raw");
    ok &= expect_true(!owner.owner_connected, "snapshot owner connected false sync");

    GBE_DotaLobbyMemberSnapshotData remote = gbe::dota_lobby_flow::compose_lobby_member_snapshot_data(
        20ull,
        20u,
        10ull,
        100u,
        kGoodGuys,
        1u,
        11u,
        true,
        false,
        0u,
        false,
        0u,
        22u,
        true,
        kPlayerPool);
    ok &= expect_eq_u64(remote.member.team, kPlayerPool, "snapshot remote defaults to player pool");
    ok &= expect_eq_u64(remote.member.slot, 0u, "snapshot remote parsed default slot");
    ok &= expect_eq_u64(remote.member.hero_id, 22u, "snapshot remote hero");
    ok &= expect_true(remote.member.connected, "snapshot remote connected");
    ok &= expect_eq_u64(remote.owner_team, kGoodGuys, "snapshot remote keeps owner team");
    ok &= expect_eq_u64(remote.owner_slot, 1u, "snapshot remote keeps owner slot");

    remote = gbe::dota_lobby_flow::compose_lobby_member_snapshot_data(
        20ull,
        20u,
        10ull,
        100u,
        kGoodGuys,
        1u,
        11u,
        true,
        true,
        3u,
        true,
        6u,
        23u,
        false,
        kPlayerPool);
    ok &= expect_eq_u64(remote.member.team, 3u, "snapshot remote parsed team");
    ok &= expect_eq_u64(remote.member.slot, 6u, "snapshot remote parsed slot");
    ok &= expect_eq_u64(remote.member.hero_id, 23u, "snapshot remote parsed hero");
    ok &= expect_true(!remote.member.connected, "snapshot remote parsed disconnected");

    return ok;
}

bool test_coordinator_generic_snapshot_flow_block()
{
    bool ok = true;

    GBE_DotaLobbyOwnerSnapshotData owner_snapshot = gbe::dota_lobby_flow::compose_lobby_owner_snapshot_data(
        10ull,
        10u,
        "Stored Owner",
        20ull,
        20u,
        true,
        20ull,
        "Local",
        99u,
        "Lobby Host");
    ok &= expect_eq_u64(owner_snapshot.owner_steam_id, 20ull, "coordinator snapshot owner steam");
    ok &= expect_eq_u64(owner_snapshot.owner_account_id, 20u, "coordinator snapshot owner account");
    ok &= expect_true(owner_snapshot.owner_name == "Local", "coordinator snapshot owner name");
    ok &= expect_true(owner_snapshot.should_publish_local_owner, "coordinator snapshot owner publish local");

    GBE_DotaLobbyOwnerPublishData owner_publish = gbe::dota_lobby_flow::compose_lobby_owner_publish_data(owner_snapshot, "Local");
    ok &= expect_eq_u64(owner_publish.owner_steam_id, 20ull, "coordinator publish owner steam");
    ok &= expect_true(owner_publish.owner_name == "Local", "coordinator publish owner name");

    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u), member(30ull, 3u)};
    members[1].connected = false;
    members[1].leaver_status = 1u;

    std::uint64_t owner_steam_id = 10ull;
    std::uint32_t owner_account_id = 10u;
    std::uint32_t owner_team = kGoodGuys;
    std::uint32_t owner_slot = 1u;
    std::uint32_t owner_hero_id = 11u;
    bool owner_connected = true;

    GBE_DotaLobbyMemberState adopted_owner = gbe::dota_lobby_flow::adopt_lobby_owner_member(
        members,
        owner_snapshot.owner_steam_id,
        owner_snapshot.owner_account_id,
        kGoodGuys,
        false,
        owner_steam_id,
        owner_account_id,
        owner_team,
        owner_slot,
        owner_hero_id,
        owner_connected);
    ok &= expect_eq_u64(adopted_owner.steam_id, 20ull, "coordinator adoption owner steam");
    ok &= expect_eq_u64(owner_steam_id, 20ull, "coordinator adoption owner steam field");
    ok &= expect_eq_u64(owner_slot, 0u, "coordinator adoption owner slot field");
    ok &= expect_true(!owner_connected, "coordinator adoption owner connected field");

    GBE_DotaGenericLobbyMemberSnapshotInput owner_input{};
    owner_input.member_steam_id = 20ull;
    owner_input.member_account_id = 20u;
    owner_input.owner_steam_id = owner_steam_id;
    owner_input.owner_account_id = owner_account_id;
    owner_input.owner_team = owner_team;
    owner_input.owner_slot = owner_slot;
    owner_input.owner_hero_id = owner_hero_id;
    owner_input.owner_connected = owner_connected;
    owner_input.member_team_raw = "0";
    owner_input.member_slot_raw = "0";
    owner_input.member_hero_raw = "42";
    owner_input.member_connected_raw = "0";
    owner_input.player_pool_team = kPlayerPool;
    GBE_DotaLobbyMemberSnapshotData owner_member_snapshot = gbe::dota_lobby_flow::compose_generic_lobby_member_snapshot_data(owner_input);

    GBE_DotaLobbyMemberState owner_member = owner_member_snapshot.member;
    gbe::dota_lobby_flow::update_generic_lobby_member_snapshot_state(
        owner_member,
        members,
        true,
        owner_steam_id,
        owner_slot,
        members,
        true,
        true,
        kGoodGuys,
        kPlayerPool);
    gbe::dota_lobby_flow::upsert_lobby_member(members, owner_member);

    GBE_DotaGenericLobbyMemberSnapshotInput remote_input{};
    remote_input.member_steam_id = 40ull;
    remote_input.member_account_id = 40u;
    remote_input.owner_steam_id = owner_steam_id;
    remote_input.owner_account_id = owner_account_id;
    remote_input.owner_team = owner_team;
    remote_input.owner_slot = owner_slot;
    remote_input.owner_hero_id = owner_hero_id;
    remote_input.owner_connected = owner_connected;
    remote_input.member_team_raw.clear();
    remote_input.member_slot_raw = "4";
    remote_input.member_hero_raw = "23";
    remote_input.member_connected_raw = "1";
    remote_input.player_pool_team = kPlayerPool;
    GBE_DotaLobbyMemberSnapshotData remote_member_snapshot = gbe::dota_lobby_flow::compose_generic_lobby_member_snapshot_data(remote_input);

    GBE_DotaLobbyMemberState remote_member = remote_member_snapshot.member;
    gbe::dota_lobby_flow::update_generic_lobby_member_snapshot_state(
        remote_member,
        members,
        true,
        owner_steam_id,
        owner_slot,
        members,
        true,
        true,
        kGoodGuys,
        kPlayerPool);
    gbe::dota_lobby_flow::upsert_lobby_member(members, remote_member);

    ok &= expect_eq_u64(members.size(), 4u, "coordinator flow keeps adopted owner plus remote members");
    ok &= expect_eq_u64(members[2].steam_id, 20ull, "coordinator flow adopted owner appended");
    ok &= expect_eq_u64(members[2].slot, 0u, "coordinator flow adopted owner slot cleared");
    ok &= expect_eq_u64(members[3].steam_id, 40ull, "coordinator flow remote member appended");
    ok &= expect_eq_u64(members[3].team, kGoodGuys, "coordinator flow remote normalized team");
    ok &= expect_eq_u64(members[3].slot, 2u, "coordinator flow remote normalized slot");

    return ok;
}

bool test_compose_lobby_members()
{
    bool ok = true;

    std::vector<GBE_DotaLobbyMemberState> members;
    GBE_DotaLobbyMemberState pool_member = member(20ull, 7u);
    pool_member.team = kPlayerPool;
    pool_member.account_id = 0u;
    members.push_back(pool_member);
    members.push_back(GBE_DotaLobbyMemberState{});

    std::vector<GBE_DotaLobbyMemberState> composed = gbe::dota_lobby_flow::compose_lobby_members(10ull, 100u, kGoodGuys, 1u, 42u, true, kPlayerPool, members);
    ok &= expect_eq_u64(composed.size(), 3u, "compose inserts missing owner");
    ok &= expect_eq_u64(composed[0].steam_id, 10ull, "compose owner steam id");
    ok &= expect_eq_u64(composed[0].account_id, 100u, "compose owner account id");
    ok &= expect_eq_u64(composed[0].hero_id, 42u, "compose owner hero id");
    ok &= expect_eq_u64(composed[1].account_id, 20u, "compose derives member account id");
    ok &= expect_eq_u64(composed[1].slot, 0u, "compose clears player pool slot");
    ok &= expect_eq_u64(composed[2].steam_id, 0ull, "compose preserves zero member");

    std::vector<GBE_DotaLobbyMemberState> owner_members{member(10ull, 9u), member(20ull, 2u), member(20ull, 5u)};
    owner_members[0].team = kPlayerPool;
    owner_members[0].connected = false;
    std::vector<GBE_DotaLobbyMemberState> overridden = gbe::dota_lobby_flow::compose_lobby_members(10ull, 101u, kGoodGuys, 1u, 77u, true, kPlayerPool, owner_members);
    ok &= expect_eq_u64(overridden.size(), 2u, "compose updates duplicate member");
    ok &= expect_eq_u64(overridden[0].account_id, 101u, "compose overrides owner account id");
    ok &= expect_eq_u64(overridden[0].slot, 1u, "compose overrides owner slot");
    ok &= expect_eq_u64(overridden[0].hero_id, 77u, "compose overrides owner hero");
    ok &= expect_true(overridden[0].connected, "compose overrides owner connected");
    ok &= expect_eq_u64(overridden[1].slot, 5u, "compose keeps last duplicate");

    return ok;
}

bool test_normalize_and_owner_transfer()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u), member(20ull, 2u)};

    GBE_DotaLobbyMemberState pool = member(30ull, 0u);
    pool.team = kPlayerPool;
    ok &= expect_true(gbe::dota_lobby_flow::normalize_arcade_lobby_member_slot(pool, members, 10ull, 1u, kGoodGuys, kPlayerPool), "normalize pool member");
    ok &= expect_eq_u64(pool.team, kGoodGuys, "normalized team");
    ok &= expect_eq_u64(pool.slot, 3u, "normalized slot");

    GBE_DotaLobbyMemberState owner = member(10ull, 1u);
    ok &= expect_true(!gbe::dota_lobby_flow::normalize_arcade_lobby_member_slot(owner, members, 10ull, 1u, kGoodGuys, kPlayerPool), "skip owner normalize");

    std::vector<GBE_DotaLobbyMemberState> previous{member(10ull, 1u), member(20ull, 2u), member(30ull, 3u)};
    std::vector<GBE_DotaLobbyMemberState> current{member(20ull, 1u), member(30ull, 3u), member(40ull, 4u)};
    gbe::dota_lobby_flow::preserve_lobby_owner_transfer_slots(current, previous, 10ull, 20ull);
    ok &= expect_eq_u64(current.size(), 4u, "owner transfer size");
    ok &= expect_eq_u64(current[1].steam_id, 20ull, "new owner old index");
    ok &= expect_eq_u64(current[2].steam_id, 30ull, "preserve existing member index");
    ok &= expect_eq_u64(current[3].steam_id, 40ull, "append new member");

    return ok;
}

bool test_normalize_arcade_lobby_member_slots()
{
    bool ok = true;
    std::uint32_t owner_team = kPlayerPool;
    std::uint32_t owner_slot = 0u;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 8u), member(20ull, 0u), member(30ull, 9u)};
    members[0].team = kPlayerPool;
    members[1].team = kPlayerPool;
    members[2].team = 3u;

    ok &= expect_true(gbe::dota_lobby_flow::normalize_arcade_lobby_member_slots(true, 10ull, owner_team, owner_slot, members, kGoodGuys), "normalize arcade slots changed");
    ok &= expect_eq_u64(owner_team, kGoodGuys, "normalize owner team");
    ok &= expect_eq_u64(owner_slot, 1u, "normalize owner slot");
    ok &= expect_eq_u64(members[0].team, kGoodGuys, "normalize owner member team");
    ok &= expect_eq_u64(members[0].slot, 1u, "normalize owner member slot");
    ok &= expect_eq_u64(members[1].team, kGoodGuys, "normalize first remote team");
    ok &= expect_eq_u64(members[1].slot, 2u, "normalize first remote slot");
    ok &= expect_eq_u64(members[2].slot, 3u, "normalize second remote slot");

    ok &= expect_true(!gbe::dota_lobby_flow::normalize_arcade_lobby_member_slots(true, 10ull, owner_team, owner_slot, members, kGoodGuys), "normalize arcade slots stable");

    std::uint32_t non_custom_owner_team = kPlayerPool;
    std::uint32_t non_custom_owner_slot = 0u;
    std::vector<GBE_DotaLobbyMemberState> non_custom_members{member(10ull, 8u)};
    ok &= expect_true(!gbe::dota_lobby_flow::normalize_arcade_lobby_member_slots(false, 10ull, non_custom_owner_team, non_custom_owner_slot, non_custom_members, kGoodGuys), "skip non custom lobby");
    ok &= expect_eq_u64(non_custom_owner_team, kPlayerPool, "non custom owner team unchanged");
    ok &= expect_eq_u64(non_custom_owner_slot, 0u, "non custom owner slot unchanged");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_member_find_and_equality();
    ok &= test_member_diff_and_filter();
    ok &= test_count_remote_lobby_members();
    ok &= test_launch_state_peer_selection();
    ok &= test_launch_state_push_planner();
    ok &= test_launch_state_push_context_mapping();
    ok &= test_create_lobby_context_mapping();
    ok &= test_create_lobby_action_list();
    ok &= test_join_lobby_context_mapping();
    ok &= test_join_lobby_action_list();
    ok &= test_launch_state_plan_input_source_lobby_mapping();
    ok &= test_launch_state_plan_input_target_mapping();
    ok &= test_launch_state_plan_input_shared_lobby_mapping();
    ok &= test_launch_state_plan_input_capture_mapping();
    ok &= test_launch_state_captured_lobby_input_from_local_lobby();
    ok &= test_launch_state_plan_input_captured_lobby_mapping();
    ok &= test_upsert_and_slot_selection();
    ok &= test_apply_lobby_member_team_slot_update();
    ok &= test_member_state_update_block();
    ok &= test_owner_adoption_block();
    ok &= test_generic_snapshot_block();
    ok &= test_chat_channel_sync_block();
    ok &= test_details_update_payload_routing_block();
    ok &= test_cache_subscribed_payload_routing_block();
    ok &= test_authoritative_lobby_payload_data_block();
    ok &= test_lobby_publish_data_block();
    ok &= test_generic_lobby_snapshot_basic_block();
    ok &= test_generic_lobby_snapshot_member_parse_block();
    ok &= test_coordinator_generic_snapshot_flow_block();
    ok &= test_compose_lobby_members();
    ok &= test_normalize_and_owner_transfer();
    ok &= test_normalize_arcade_lobby_member_slots();

    if (!ok)
        return 1;

    std::cout << "gbe_dota_lobby_flow_test passed" << std::endl;
    return 0;
}
