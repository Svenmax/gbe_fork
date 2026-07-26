#include "gbe_dota_lobby_state.h"
#include "gbe_dota_reconnect_context.h"

#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_proto_wire.h"

#include <cstring>

namespace gbe::dota_lobby_state {

namespace {

void apply_create_lobby_details(const proto_wire::DotaPracticeLobbyDetailsRequest &details, GBE_LocalLobby &lobby)
{
    if (details.has_room_name)
        lobby.room_name = details.room_name;
    if (details.has_server_region)
        lobby.server_region = details.server_region;
    if (details.has_lan)
        lobby.lan = details.lan;
    if (details.has_lan_host_ping_location)
        lobby.lan_host_ping_location = details.lan_host_ping_location;
    if (details.has_game_mode)
        lobby.game_mode = details.game_mode;
    if (details.has_bot_difficulty_radiant)
        lobby.bot_difficulty_radiant = details.bot_difficulty_radiant;
    if (details.has_allow_cheats)
        lobby.allow_cheats = details.allow_cheats;
    if (details.has_fill_with_bots)
        lobby.fill_with_bots = details.fill_with_bots;
    if (details.has_allow_spectating)
        lobby.allow_spectating = details.allow_spectating;
    if (details.has_visibility)
        lobby.visibility = details.visibility;
    if (details.has_bot_difficulty_dire)
        lobby.bot_difficulty_dire = details.bot_difficulty_dire;
    if (details.has_bot_radiant)
        lobby.bot_radiant = details.bot_radiant;
    if (details.has_bot_dire)
        lobby.bot_dire = details.bot_dire;
    if (details.has_pass_key)
        lobby.pass_key = details.pass_key;
    if (details.has_custom_game_mode)
        lobby.custom_game.mode = details.custom_game_mode;
    if (details.has_custom_map_name)
        lobby.custom_game.map_name = details.custom_map_name;
    if (details.has_custom_difficulty)
        lobby.custom_game.difficulty = details.custom_difficulty;
    if (details.has_custom_game_id)
        lobby.custom_game.game_id = details.custom_game_id;
    if (details.has_custom_min_players)
        lobby.custom_game.min_players = details.custom_min_players;
    if (details.has_custom_max_players)
        lobby.custom_game.max_players = details.custom_max_players;
    if (details.has_custom_game_crc)
        lobby.custom_game.crc = details.custom_game_crc;
    if (details.has_custom_game_timestamp)
        lobby.custom_game.timestamp = details.custom_game_timestamp;
    if (details.has_custom_game_penalties)
        lobby.custom_game.penalties = details.custom_game_penalties;
}

} // namespace

CreateLobbyPlan compose_create_lobby_plan(
    const proto_wire::DotaPracticeLobbyCreateRequest &request,
    std::uint64_t lobby_id,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    const std::string &owner_name,
    std::uint32_t owner_team,
    std::uint32_t owner_slot)
{
    CreateLobbyPlan plan{};
    plan.lobby.active = true;
    plan.lobby.created = std::chrono::high_resolution_clock::now();
    plan.lobby.lobby_id = lobby_id;
    plan.lobby.lan = true;
    plan.lobby.fill_with_bots = true;
    plan.lobby.bot_difficulty_dire = 4u;
    plan.lobby.owner_steam_id = owner_steam_id;
    plan.lobby.owner_account_id = owner_account_id;
    plan.lobby.owner_name = owner_name;
    plan.lobby.owner_team = owner_team;
    plan.lobby.owner_slot = owner_slot;

    if (request.has_lobby_details)
        apply_create_lobby_details(request.lobby_details, plan.lobby);
    if (request.has_pass_key && plan.lobby.pass_key.empty())
        plan.lobby.pass_key = request.pass_key;

    GBE_DotaLobbyMemberState owner_member{};
    owner_member.steam_id = plan.lobby.owner_steam_id;
    owner_member.account_id = plan.lobby.owner_account_id;
    owner_member.team = plan.lobby.owner_team;
    owner_member.slot = plan.lobby.owner_slot;
    owner_member.connected = false;
    plan.lobby.members.push_back(owner_member);
    plan.custom_game_create = plan.lobby.custom_game.game_id != 0ull;
    return plan;
}

CreateLobbyStateApplyPlan compose_create_lobby_state_apply_plan(
    const CreateLobbyPlan &create_plan,
    bool has_lobby_details)
{
    CreateLobbyStateApplyPlan plan{};
    plan.lobby = create_plan.lobby;
    plan.normalize_custom_game_details = has_lobby_details;
    plan.normalize_arcade_member_slots = create_plan.custom_game_create;
    plan.clear_reconnect_context = create_plan.custom_game_create;
    plan.set_reconnect_eligible = create_plan.custom_game_create;
    plan.log_arcade_isolation = create_plan.custom_game_create;
    return plan;
}

CreateLobbyResetPlan compose_create_lobby_reset_plan(
    const GBE_LocalLobby &previous_lobby,
    const GBE_DotaCustomGameDetails &requested_custom_game)
{
    CreateLobbyResetPlan plan{};
    plan.reset_gc_memory = true;
    plan.reset_reason = "7038_create";
    plan.reset_leave_generic_lobby = true;
    plan.reset_clear_queued_messages = true;
    plan.custom_game_create = requested_custom_game.game_id != 0ull;
    plan.previous_lobby_id = previous_lobby.lobby_id;
    plan.previous_match_id = previous_lobby.match_id;
    plan.previous_custom_game_id = previous_lobby.custom_game.game_id;
    plan.previous_state = previous_lobby.state;
    plan.previous_game_state = previous_lobby.game_state;
    plan.previous_owner_team = previous_lobby.owner_team;
    plan.previous_owner_slot = previous_lobby.owner_slot;
    plan.unsubscribe_previous_practice_lobby =
        plan.custom_game_create &&
        previous_lobby.active &&
        plan.previous_lobby_id != 0ull &&
        plan.previous_match_id != 0ull &&
        plan.previous_custom_game_id == 0ull;
    return plan;
}

JoinLobbyMergePlan compose_join_lobby_merge_plan(
    const GBE_LocalLobby &current_lobby,
    bool has_request_lobby_id,
    std::uint64_t request_lobby_id,
    bool has_request_pass_key,
    const std::string &request_pass_key,
    bool matched_generic_lobby,
    const GBE_LocalLobby &matched_lobby,
    std::uint64_t local_steam_id,
    std::uint32_t local_account_id,
    const std::string &local_name,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team)
{
    JoinLobbyMergePlan plan{};
    if (!current_lobby.active || current_lobby.lobby_id == 0) {
        plan.lobby = GBE_LocalLobby{};
        plan.lobby.active = true;
        plan.lobby.lobby_id = has_request_lobby_id && request_lobby_id != 0 ? request_lobby_id : 0ull;
        plan.lobby.owner_steam_id = local_steam_id;
        plan.lobby.owner_account_id = local_account_id;
        plan.lobby.owner_name = local_name;
        plan.lobby.owner_team = good_guys_team;
        plan.lobby.owner_slot = 1u;
        plan.lobby.game_mode = 2u;
        plan.lobby.server_region = 15u;
        plan.lobby.allow_spectating = true;
        plan.lobby.bot_difficulty_dire = 4u;
        plan.lobby.room_name = "Lobby";
    } else {
        plan.lobby = current_lobby;
        if (has_request_lobby_id && request_lobby_id != 0)
            plan.lobby.lobby_id = request_lobby_id;
    }

    if (matched_generic_lobby) {
        plan.lobby.generic_lobby_id = matched_lobby.generic_lobby_id;
        plan.lobby.room_name = matched_lobby.room_name;
        plan.lobby.game_mode = matched_lobby.game_mode;
        plan.lobby.server_region = matched_lobby.server_region;
        plan.lobby.lan = matched_lobby.lan;
        plan.lobby.lan_host_ping_location = matched_lobby.lan_host_ping_location;
        plan.lobby.pass_key = matched_lobby.pass_key;
        plan.lobby.allow_cheats = matched_lobby.allow_cheats;
        plan.lobby.fill_with_bots = matched_lobby.fill_with_bots;
        plan.lobby.allow_spectating = matched_lobby.allow_spectating;
        plan.lobby.visibility = matched_lobby.visibility;
        plan.lobby.bot_difficulty_radiant = matched_lobby.bot_difficulty_radiant;
        plan.lobby.bot_difficulty_dire = matched_lobby.bot_difficulty_dire;
        plan.lobby.bot_radiant = matched_lobby.bot_radiant;
        plan.lobby.bot_dire = matched_lobby.bot_dire;
        plan.lobby.custom_game = matched_lobby.custom_game;
        plan.lobby.state = matched_lobby.state;
        plan.lobby.game_state = matched_lobby.game_state;
        plan.lobby.match_id = matched_lobby.match_id;
        plan.lobby.server_id = matched_lobby.server_id;
        plan.lobby.connect = matched_lobby.connect;
        plan.lobby.game_start_time = matched_lobby.game_start_time;
        plan.lobby.owner_account_id = matched_lobby.owner_account_id;
        plan.lobby.owner_name = matched_lobby.owner_name;
        if (matched_lobby.owner_steam_id != 0ull)
            plan.lobby.owner_steam_id = matched_lobby.owner_steam_id;
        plan.lobby.owner_team = matched_lobby.owner_team;
        plan.lobby.owner_slot = matched_lobby.owner_slot;
        plan.lobby.owner_hero_id = matched_lobby.owner_hero_id;
        plan.lobby.owner_connected = matched_lobby.owner_connected;
        plan.lobby.members = matched_lobby.members;

        GBE_DotaLobbyMemberState owner_member{};
        owner_member.steam_id = plan.lobby.owner_steam_id;
        owner_member.account_id = plan.lobby.owner_account_id;
        owner_member.team = plan.lobby.owner_team;
        owner_member.slot = plan.lobby.owner_slot;
        owner_member.hero_id = plan.lobby.owner_hero_id;
        owner_member.connected = plan.lobby.owner_connected || plan.lobby.state == 3u;
        dota_lobby_flow::upsert_lobby_member(plan.lobby.members, owner_member);
    }

    plan.local_member.steam_id = local_steam_id;
    plan.local_member.account_id = local_account_id;
    plan.local_member.team = player_pool_team;
    plan.local_member.slot = 0u;
    plan.local_member.connected = false;
    if (dota_custom_game::has_custom_game_details(plan.lobby.custom_game)) {
        dota_lobby_flow::normalize_arcade_lobby_member_slot(
            plan.local_member,
            plan.lobby.members,
            plan.lobby.owner_steam_id,
            plan.lobby.owner_slot,
            good_guys_team,
            player_pool_team);
    }

    plan.seen_local_in_generic_lobby = plan.lobby.seen_local_in_generic_lobby;
    for (const GBE_DotaLobbyMemberState &member : matched_lobby.members) {
        if (member.steam_id == plan.local_member.steam_id) {
            plan.seen_local_in_generic_lobby = true;
            break;
        }
    }
    if (has_request_pass_key)
        plan.lobby.pass_key = request_pass_key;
    plan.lobby.seen_local_in_generic_lobby = plan.seen_local_in_generic_lobby;
    dota_lobby_flow::upsert_lobby_member(plan.lobby.members, plan.local_member);
    return plan;
}

LaunchInitPlan compose_launch_init_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint64_t match_id,
    std::uint64_t server_id,
    const std::string &connect,
    std::uint32_t game_start_time,
    std::uint32_t requested_launch_phase)
{
    LaunchInitPlan plan{};
    plan.lobby = current_lobby;
    plan.lobby.match_id = match_id;
    plan.lobby.server_id = server_id;
    if (dota_gc_wire::should_prefer_dota_lobby_connect_update(plan.lobby.connect, connect))
        plan.lobby.connect = connect;
    plan.lobby.game_start_time = game_start_time;
    plan.lobby.launch_phase = requested_launch_phase;
    return plan;
}

CustomGameLaunchSetupPlan compose_custom_game_launch_setup_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t synced_launch_phase)
{
    CustomGameLaunchSetupPlan plan{};
    plan.readyup_lobby = current_lobby;
    plan.readyup_lobby.state = 4u;
    plan.readyup_lobby.game_state = 0u;
    plan.serversetup_lobby = current_lobby;
    plan.serversetup_lobby.state = 1u;
    plan.serversetup_lobby.game_state = 0u;
    plan.synced_launch_phase = synced_launch_phase;
    return plan;
}

bool has_launch_server_setup_sync(const GBE_LocalLobby &current_lobby)
{
    return
        current_lobby.active &&
        current_lobby.lobby_id != 0ull &&
        current_lobby.match_id != 0ull &&
        current_lobby.game_start_time != 0u &&
        !current_lobby.connect.empty();
}

bool advance_launch_phase(GBE_LocalLobby &lobby, std::uint32_t phase)
{
    if (lobby.launch_phase >= phase)
        return false;

    lobby.launch_phase = phase;
    return true;
}

LaunchRunPlan compose_launch_run_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase,
    std::uint32_t next_game_state)
{
    LaunchRunPlan plan{};
    if (!has_launch_server_setup_sync(current_lobby) || current_lobby.launch_phase < setup_synced_launch_phase)
        return plan;

    plan.can_advance = true;
    plan.launch_phase = current_lobby.launch_phase < run_queued_launch_phase ? run_queued_launch_phase : current_lobby.launch_phase;
    plan.next_state = 2u;
    plan.next_game_state = next_game_state;
    return plan;
}

QueuedLobbyStateApplyPlan compose_queued_lobby_state_apply_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t queued_state,
    std::uint32_t queued_game_state,
    bool preserve_monotonic_game_state,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase)
{
    QueuedLobbyStateApplyPlan plan{};
    plan.state = queued_state;
    plan.game_state = queued_game_state;
    plan.launch_phase = current_lobby.launch_phase;

    if (preserve_monotonic_game_state &&
        current_lobby.active &&
        current_lobby.state == 2u &&
        queued_state == 2u &&
        current_lobby.game_state > 0u &&
        plan.game_state == 0u) {
        plan.game_state = current_lobby.game_state;
        plan.preserved_game_state = true;
    }

    if (plan.state == 1u && plan.game_state == 0u && has_launch_server_setup_sync(current_lobby)) {
        if (plan.launch_phase < setup_synced_launch_phase)
            plan.launch_phase = setup_synced_launch_phase;
    } else if (plan.state == 2u && plan.game_state == 0u) {
        if (plan.launch_phase < run_queued_launch_phase)
            plan.launch_phase = run_queued_launch_phase;
    }

    return plan;
}

void apply_queued_lobby_state_apply_plan(
    GBE_LocalLobby &lobby,
    const QueuedLobbyStateApplyPlan &plan)
{
    lobby.state = plan.state;
    lobby.game_state = plan.game_state;
    lobby.launch_phase = plan.launch_phase;
}

GenericLobbyStateCapturePlan compose_generic_lobby_state_capture_plan(
    const GBE_LocalLobby &current_lobby,
    bool has_generic_state,
    std::uint32_t generic_state,
    bool has_generic_game_state,
    std::uint32_t generic_game_state,
    std::uint32_t setup_synced_launch_phase)
{
    const bool protect_launch_progress =
        current_lobby.custom_game.game_id != 0ull &&
        current_lobby.match_id != 0ull &&
        current_lobby.launch_phase >= setup_synced_launch_phase;
    GenericLobbyStateCapturePlan plan{};
    plan.state = generic_state;
    plan.game_state = generic_game_state;
    plan.ignored_stale_state = has_generic_state && protect_launch_progress && generic_state < current_lobby.state;
    plan.apply_state = has_generic_state && !plan.ignored_stale_state;
    plan.apply_game_state =
        has_generic_game_state &&
        !(protect_launch_progress && generic_game_state < current_lobby.game_state);
    return plan;
}

void apply_generic_lobby_state_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyStateCapturePlan &plan)
{
    if (plan.apply_state)
        lobby.state = plan.state;
    if (plan.apply_game_state)
        lobby.game_state = plan.game_state;
}

GenericLobbyRuntimeIdentityCapturePlan compose_generic_lobby_runtime_identity_capture_plan(
    const GBE_LocalLobby &current_lobby,
    const std::string &generic_room_name,
    const std::string &generic_match_id_raw,
    const std::string &generic_server_id_raw,
    const std::string &generic_connect,
    const std::string &generic_game_start_time_raw)
{
    GenericLobbyRuntimeIdentityCapturePlan plan{};
    plan.room_name = generic_room_name;
    plan.apply_room_name = !generic_room_name.empty();

    plan.match_id = proto_wire::parse_uint64_or_zero(generic_match_id_raw.c_str());
    plan.apply_match_id =
        !generic_match_id_raw.empty() &&
        (plan.match_id != 0ull || current_lobby.match_id == 0ull);
    const std::uint64_t effective_match_id =
        plan.apply_match_id ? plan.match_id : current_lobby.match_id;

    plan.preserve_existing_lan_runtime =
        current_lobby.custom_game.game_id == 0ull &&
        current_lobby.lan &&
        effective_match_id != 0ull &&
        current_lobby.server_id != 0ull &&
        proto_wire::parse_dota_practice_lobby_connect_ipv4(current_lobby.connect) != 0u;

    plan.server_id = proto_wire::parse_uint64_or_zero(generic_server_id_raw.c_str());
    plan.apply_server_id =
        !generic_server_id_raw.empty() &&
        !plan.preserve_existing_lan_runtime &&
        (plan.server_id != 0ull ||
         current_lobby.server_id == 0ull ||
         effective_match_id == 0ull);

    plan.connect = proto_wire::normalize_dota_practice_lobby_connect(generic_connect);
    plan.apply_connect = !generic_connect.empty() && !plan.preserve_existing_lan_runtime;

    plan.game_start_time =
        proto_wire::parse_uint32_or_zero(generic_game_start_time_raw.c_str());
    plan.apply_game_start_time = !generic_game_start_time_raw.empty();
    return plan;
}

void apply_generic_lobby_runtime_identity_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyRuntimeIdentityCapturePlan &plan)
{
    if (plan.apply_room_name)
        lobby.room_name = plan.room_name;
    if (plan.apply_match_id)
        lobby.match_id = plan.match_id;
    if (plan.apply_server_id)
        lobby.server_id = plan.server_id;
    if (plan.apply_connect)
        lobby.connect = plan.connect;
    if (plan.apply_game_start_time)
        lobby.game_start_time = plan.game_start_time;
}

GenericLobbyOptionsCapturePlan compose_generic_lobby_options_capture_plan(
    const std::string &generic_allow_cheats_raw,
    const std::string &generic_fill_with_bots_raw,
    const std::string &generic_allow_spectating_raw,
    const std::string &generic_visibility_raw,
    const std::string &generic_bot_difficulty_radiant_raw,
    const std::string &generic_bot_difficulty_dire_raw,
    const std::string &generic_bot_radiant_raw,
    const std::string &generic_bot_dire_raw)
{
    GenericLobbyOptionsCapturePlan plan{};
    plan.apply_allow_cheats = !generic_allow_cheats_raw.empty();
    plan.allow_cheats =
        proto_wire::parse_uint32_or_zero(generic_allow_cheats_raw.c_str()) != 0u;
    plan.apply_fill_with_bots = !generic_fill_with_bots_raw.empty();
    plan.fill_with_bots =
        proto_wire::parse_uint32_or_zero(generic_fill_with_bots_raw.c_str()) != 0u;
    plan.apply_allow_spectating = !generic_allow_spectating_raw.empty();
    plan.allow_spectating =
        proto_wire::parse_uint32_or_zero(generic_allow_spectating_raw.c_str()) != 0u;
    plan.apply_visibility = !generic_visibility_raw.empty();
    plan.visibility = proto_wire::parse_uint32_or_zero(generic_visibility_raw.c_str());
    plan.apply_bot_difficulty_radiant = !generic_bot_difficulty_radiant_raw.empty();
    plan.bot_difficulty_radiant =
        proto_wire::parse_uint32_or_zero(generic_bot_difficulty_radiant_raw.c_str());
    plan.apply_bot_difficulty_dire = !generic_bot_difficulty_dire_raw.empty();
    plan.bot_difficulty_dire =
        proto_wire::parse_uint32_or_zero(generic_bot_difficulty_dire_raw.c_str());
    plan.apply_bot_radiant = !generic_bot_radiant_raw.empty();
    plan.bot_radiant = proto_wire::parse_uint64_or_zero(generic_bot_radiant_raw.c_str());
    plan.apply_bot_dire = !generic_bot_dire_raw.empty();
    plan.bot_dire = proto_wire::parse_uint64_or_zero(generic_bot_dire_raw.c_str());
    return plan;
}

void apply_generic_lobby_options_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyOptionsCapturePlan &plan)
{
    if (plan.apply_allow_cheats)
        lobby.allow_cheats = plan.allow_cheats;
    if (plan.apply_fill_with_bots)
        lobby.fill_with_bots = plan.fill_with_bots;
    if (plan.apply_allow_spectating)
        lobby.allow_spectating = plan.allow_spectating;
    if (plan.apply_visibility)
        lobby.visibility = plan.visibility;
    if (plan.apply_bot_difficulty_radiant)
        lobby.bot_difficulty_radiant = plan.bot_difficulty_radiant;
    if (plan.apply_bot_difficulty_dire)
        lobby.bot_difficulty_dire = plan.bot_difficulty_dire;
    if (plan.apply_bot_radiant)
        lobby.bot_radiant = plan.bot_radiant;
    if (plan.apply_bot_dire)
        lobby.bot_dire = plan.bot_dire;
}

GenericLobbyCustomGameCapturePlan compose_generic_lobby_custom_game_capture_plan(
    const std::string &generic_custom_game_mode,
    const std::string &generic_custom_map_name,
    const std::string &generic_custom_difficulty_raw,
    const std::string &generic_custom_game_id_raw,
    const std::string &generic_custom_min_players_raw,
    const std::string &generic_custom_max_players_raw,
    const std::string &generic_custom_game_crc_raw,
    const std::string &generic_custom_game_timestamp_raw,
    const std::string &generic_custom_game_penalties_raw)
{
    GenericLobbyCustomGameCapturePlan plan{};
    plan.apply_mode = !generic_custom_game_mode.empty();
    plan.mode = generic_custom_game_mode;
    plan.apply_map_name = !generic_custom_map_name.empty();
    plan.map_name = generic_custom_map_name;
    plan.apply_difficulty = !generic_custom_difficulty_raw.empty();
    plan.difficulty = proto_wire::parse_uint32_or_zero(generic_custom_difficulty_raw.c_str());
    plan.apply_game_id = !generic_custom_game_id_raw.empty();
    plan.game_id = proto_wire::parse_uint64_or_zero(generic_custom_game_id_raw.c_str());
    plan.apply_min_players = !generic_custom_min_players_raw.empty();
    plan.min_players = proto_wire::parse_uint32_or_zero(generic_custom_min_players_raw.c_str());
    plan.apply_max_players = !generic_custom_max_players_raw.empty();
    plan.max_players = proto_wire::parse_uint32_or_zero(generic_custom_max_players_raw.c_str());
    plan.apply_crc = !generic_custom_game_crc_raw.empty();
    plan.crc = proto_wire::parse_uint64_or_zero(generic_custom_game_crc_raw.c_str());
    plan.apply_timestamp = !generic_custom_game_timestamp_raw.empty();
    plan.timestamp = proto_wire::parse_uint32_or_zero(generic_custom_game_timestamp_raw.c_str());
    plan.apply_penalties = !generic_custom_game_penalties_raw.empty();
    plan.penalties =
        proto_wire::parse_uint32_or_zero(generic_custom_game_penalties_raw.c_str()) != 0u;
    return plan;
}

void apply_generic_lobby_custom_game_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyCustomGameCapturePlan &plan)
{
    if (plan.apply_mode)
        lobby.custom_game.mode = plan.mode;
    if (plan.apply_map_name)
        lobby.custom_game.map_name = plan.map_name;
    if (plan.apply_difficulty)
        lobby.custom_game.difficulty = plan.difficulty;
    if (plan.apply_game_id)
        lobby.custom_game.game_id = plan.game_id;
    if (plan.apply_min_players)
        lobby.custom_game.min_players = plan.min_players;
    if (plan.apply_max_players)
        lobby.custom_game.max_players = plan.max_players;
    if (plan.apply_crc)
        lobby.custom_game.crc = plan.crc;
    if (plan.apply_timestamp)
        lobby.custom_game.timestamp = plan.timestamp;
    if (plan.apply_penalties)
        lobby.custom_game.penalties = plan.penalties;
}

GenericLobbyCapturePlan compose_generic_lobby_capture_plan(
    const GBE_LocalLobby &current_lobby,
    const GenericLobbyCaptureInput &input,
    std::uint32_t setup_synced_launch_phase)
{
    GenericLobbyCapturePlan plan{};
    plan.state = compose_generic_lobby_state_capture_plan(
        current_lobby,
        input.has_state,
        input.state,
        input.has_game_state,
        input.game_state,
        setup_synced_launch_phase);
    plan.runtime_identity = compose_generic_lobby_runtime_identity_capture_plan(
        current_lobby,
        input.room_name,
        input.match_id_raw,
        input.server_id_raw,
        input.connect,
        input.game_start_time_raw);
    plan.options = compose_generic_lobby_options_capture_plan(
        input.allow_cheats_raw,
        input.fill_with_bots_raw,
        input.allow_spectating_raw,
        input.visibility_raw,
        input.bot_difficulty_radiant_raw,
        input.bot_difficulty_dire_raw,
        input.bot_radiant_raw,
        input.bot_dire_raw);
    plan.custom_game = compose_generic_lobby_custom_game_capture_plan(
        input.custom_game_mode,
        input.custom_map_name,
        input.custom_difficulty_raw,
        input.custom_game_id_raw,
        input.custom_min_players_raw,
        input.custom_max_players_raw,
        input.custom_game_crc_raw,
        input.custom_game_timestamp_raw,
        input.custom_game_penalties_raw);
    return plan;
}

void apply_generic_lobby_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyCapturePlan &plan)
{
    apply_generic_lobby_state_capture_plan(lobby, plan.state);
    apply_generic_lobby_runtime_identity_capture_plan(lobby, plan.runtime_identity);
    apply_generic_lobby_options_capture_plan(lobby, plan.options);
    apply_generic_lobby_custom_game_capture_plan(lobby, plan.custom_game);
    if (plan.state.apply_state || plan.state.apply_game_state)
        lobby.generic_launch_runtime_generation = lobby.generation;
    if (plan.runtime_identity.apply_room_name || plan.runtime_identity.apply_match_id ||
        plan.runtime_identity.apply_server_id || plan.runtime_identity.apply_connect ||
        plan.runtime_identity.apply_game_start_time)
        lobby.generic_runtime_identity_generation = lobby.generation;
}

SourceAwareSharedRuntimeRestorePlan compose_source_aware_shared_runtime_restore_plan(
    const GBE_LocalLobby &current_lobby,
    const GBE_SharedDotaLobbyState &shared_lobby,
    std::uint32_t run_queued_launch_phase)
{
    SourceAwareSharedRuntimeRestorePlan plan{};
    const bool preserve_local_launch_runtime =
        current_lobby.generic_launch_runtime_generation != 0ull &&
        current_lobby.generic_launch_runtime_generation == shared_lobby.generation;
    const bool preserve_local_runtime_identity =
        current_lobby.generic_runtime_identity_generation != 0ull &&
        current_lobby.generic_runtime_identity_generation == shared_lobby.generation;
    plan.ignored_readyup_regression =
        dota_custom_game::has_custom_game_details(current_lobby.custom_game) &&
        current_lobby.match_id != 0ull &&
        current_lobby.launch_phase >= run_queued_launch_phase &&
        current_lobby.state == 2u &&
        current_lobby.game_state >= 2u &&
        shared_lobby.state == 4u;
    plan.state = shared_lobby.state;
    plan.game_state = shared_lobby.game_state;
    plan.launch_phase = shared_lobby.launch_phase;
    plan.apply_state = current_lobby.state != plan.state &&
        !plan.ignored_readyup_regression && !preserve_local_launch_runtime;
    plan.apply_game_state = current_lobby.game_state != plan.game_state &&
        !preserve_local_launch_runtime;
    plan.apply_launch_phase = current_lobby.launch_phase != plan.launch_phase &&
        !preserve_local_launch_runtime;
    plan.state_source = plan.ignored_readyup_regression
        ? SharedLobbyRestoreSource::ReadyupRegression
        : (preserve_local_launch_runtime
            ? SharedLobbyRestoreSource::LocalGenericCapture
            : SharedLobbyRestoreSource::SharedSnapshot);
    plan.game_state_source = preserve_local_launch_runtime
        ? SharedLobbyRestoreSource::LocalGenericCapture
        : SharedLobbyRestoreSource::SharedSnapshot;
    plan.launch_phase_source = preserve_local_launch_runtime
        ? SharedLobbyRestoreSource::LocalGenericCapture
        : SharedLobbyRestoreSource::SharedSnapshot;

    plan.room_name = shared_lobby.room_name;
    plan.connect = proto_wire::normalize_dota_practice_lobby_connect(shared_lobby.connect);
    plan.match_id = shared_lobby.match_id;
    plan.server_id = shared_lobby.server_id;
    plan.game_start_time = shared_lobby.game_start_time;
    plan.apply_room_name = current_lobby.room_name != plan.room_name &&
        !preserve_local_runtime_identity;
    plan.apply_connect = !plan.connect.empty() && current_lobby.connect != plan.connect &&
        !preserve_local_runtime_identity;
    plan.apply_match_id = plan.match_id != 0ull && current_lobby.match_id != plan.match_id &&
        !preserve_local_runtime_identity;
    plan.apply_server_id = plan.server_id != 0ull && current_lobby.server_id != plan.server_id &&
        !preserve_local_runtime_identity;
    plan.apply_game_start_time = plan.game_start_time != 0u &&
        current_lobby.game_start_time != plan.game_start_time && !preserve_local_runtime_identity;
    const SharedLobbyRestoreSource runtime_identity_source = preserve_local_runtime_identity
        ? SharedLobbyRestoreSource::LocalGenericCapture
        : SharedLobbyRestoreSource::SharedSnapshot;
    plan.room_name_source = runtime_identity_source;
    plan.connect_source = runtime_identity_source;
    plan.match_id_source = runtime_identity_source;
    plan.server_id_source = runtime_identity_source;
    plan.game_start_time_source = runtime_identity_source;
    return plan;
}

bool apply_source_aware_shared_runtime_restore_plan(
    GBE_LocalLobby &lobby,
    const SourceAwareSharedRuntimeRestorePlan &plan)
{
    bool changed = false;
    if (plan.apply_state) {
        lobby.state = plan.state;
        changed = true;
    }
    if (plan.apply_game_state) {
        lobby.game_state = plan.game_state;
        changed = true;
    }
    if (plan.apply_launch_phase) {
        lobby.launch_phase = plan.launch_phase;
        changed = true;
    }
    if (plan.apply_room_name) {
        lobby.room_name = plan.room_name;
        changed = true;
    }
    if (plan.apply_connect) {
        lobby.connect = plan.connect;
        changed = true;
    }
    if (plan.apply_match_id) {
        lobby.match_id = plan.match_id;
        changed = true;
    }
    if (plan.apply_server_id) {
        lobby.server_id = plan.server_id;
        changed = true;
    }
    if (plan.apply_game_start_time) {
        lobby.game_start_time = plan.game_start_time;
        changed = true;
    }
    return changed;
}

SharedLobbyOptionsRestorePlan compose_shared_lobby_options_restore_plan(
    const GBE_LocalLobby &current_lobby,
    const GBE_SharedDotaLobbyState &shared_lobby)
{
    SharedLobbyOptionsRestorePlan plan{};
    plan.game_mode = shared_lobby.game_mode;
    plan.apply_game_mode = current_lobby.game_mode != plan.game_mode;
    plan.server_region = shared_lobby.server_region;
    plan.apply_server_region = current_lobby.server_region != plan.server_region;
    plan.lan = shared_lobby.lan;
    plan.apply_lan = current_lobby.lan != plan.lan;
    plan.lan_host_ping_location = shared_lobby.lan_host_ping_location;
    plan.apply_lan_host_ping_location =
        current_lobby.lan_host_ping_location != plan.lan_host_ping_location;
    plan.allow_cheats = shared_lobby.allow_cheats;
    plan.apply_allow_cheats = current_lobby.allow_cheats != plan.allow_cheats;
    plan.fill_with_bots = shared_lobby.fill_with_bots;
    plan.apply_fill_with_bots = current_lobby.fill_with_bots != plan.fill_with_bots;
    plan.allow_spectating = shared_lobby.allow_spectating;
    plan.apply_allow_spectating = current_lobby.allow_spectating != plan.allow_spectating;
    plan.pass_key = shared_lobby.pass_key;
    plan.apply_pass_key = current_lobby.pass_key != plan.pass_key;
    plan.visibility = shared_lobby.visibility;
    plan.apply_visibility = current_lobby.visibility != plan.visibility;
    plan.bot_difficulty_radiant = shared_lobby.bot_difficulty_radiant;
    plan.apply_bot_difficulty_radiant =
        current_lobby.bot_difficulty_radiant != plan.bot_difficulty_radiant;
    plan.bot_difficulty_dire = shared_lobby.bot_difficulty_dire;
    plan.apply_bot_difficulty_dire =
        current_lobby.bot_difficulty_dire != plan.bot_difficulty_dire;
    plan.bot_radiant = shared_lobby.bot_radiant;
    plan.apply_bot_radiant = current_lobby.bot_radiant != plan.bot_radiant;
    plan.bot_dire = shared_lobby.bot_dire;
    plan.apply_bot_dire = current_lobby.bot_dire != plan.bot_dire;
    return plan;
}

bool apply_shared_lobby_options_restore_plan(
    GBE_LocalLobby &lobby,
    const SharedLobbyOptionsRestorePlan &plan)
{
    bool changed = false;
    if (plan.apply_game_mode) {
        lobby.game_mode = plan.game_mode;
        changed = true;
    }
    if (plan.apply_server_region) {
        lobby.server_region = plan.server_region;
        changed = true;
    }
    if (plan.apply_lan) {
        lobby.lan = plan.lan;
        changed = true;
    }
    if (plan.apply_lan_host_ping_location) {
        lobby.lan_host_ping_location = plan.lan_host_ping_location;
        changed = true;
    }
    if (plan.apply_allow_cheats) {
        lobby.allow_cheats = plan.allow_cheats;
        changed = true;
    }
    if (plan.apply_fill_with_bots) {
        lobby.fill_with_bots = plan.fill_with_bots;
        changed = true;
    }
    if (plan.apply_allow_spectating) {
        lobby.allow_spectating = plan.allow_spectating;
        changed = true;
    }
    if (plan.apply_pass_key) {
        lobby.pass_key = plan.pass_key;
        changed = true;
    }
    if (plan.apply_visibility) {
        lobby.visibility = plan.visibility;
        changed = true;
    }
    if (plan.apply_bot_difficulty_radiant) {
        lobby.bot_difficulty_radiant = plan.bot_difficulty_radiant;
        changed = true;
    }
    if (plan.apply_bot_difficulty_dire) {
        lobby.bot_difficulty_dire = plan.bot_difficulty_dire;
        changed = true;
    }
    if (plan.apply_bot_radiant) {
        lobby.bot_radiant = plan.bot_radiant;
        changed = true;
    }
    if (plan.apply_bot_dire) {
        lobby.bot_dire = plan.bot_dire;
        changed = true;
    }
    return changed;
}

SharedLobbyCacheRestorePlan compose_shared_lobby_cache_restore_plan(
    const GBE_LocalLobby &current_lobby,
    const GBE_SharedDotaLobbyState &shared_lobby)
{
    SharedLobbyCacheRestorePlan plan{};
    plan.has_cache_version = shared_lobby.has_cache_version;
    plan.cache_version = shared_lobby.cache_version;
    plan.apply_cache_version =
        current_lobby.has_cache_version != plan.has_cache_version ||
        current_lobby.cache_version != plan.cache_version;
    plan.has_cache_service_id = shared_lobby.has_cache_service_id;
    plan.cache_service_id = shared_lobby.cache_service_id;
    plan.apply_cache_service_id =
        current_lobby.has_cache_service_id != plan.has_cache_service_id ||
        current_lobby.cache_service_id != plan.cache_service_id;
    plan.cache_service_list = shared_lobby.cache_service_list;
    plan.apply_cache_service_list =
        current_lobby.cache_service_list != plan.cache_service_list;
    plan.has_cache_sync_version = shared_lobby.has_cache_sync_version;
    plan.cache_sync_version = shared_lobby.cache_sync_version;
    plan.apply_cache_sync_version =
        current_lobby.has_cache_sync_version != plan.has_cache_sync_version ||
        current_lobby.cache_sync_version != plan.cache_sync_version;
    return plan;
}

bool apply_shared_lobby_cache_restore_plan(
    GBE_LocalLobby &lobby,
    const SharedLobbyCacheRestorePlan &plan)
{
    bool changed = false;
    if (plan.apply_cache_version) {
        lobby.has_cache_version = plan.has_cache_version;
        lobby.cache_version = plan.cache_version;
        changed = true;
    }
    if (plan.apply_cache_service_id) {
        lobby.has_cache_service_id = plan.has_cache_service_id;
        lobby.cache_service_id = plan.cache_service_id;
        changed = true;
    }
    if (plan.apply_cache_service_list) {
        lobby.cache_service_list = plan.cache_service_list;
        changed = true;
    }
    if (plan.apply_cache_sync_version) {
        lobby.has_cache_sync_version = plan.has_cache_sync_version;
        lobby.cache_sync_version = plan.cache_sync_version;
        changed = true;
    }
    return changed;
}

bool apply_cache_subscription_metadata(
    GBE_LocalLobby &lobby,
    bool has_cache_version,
    std::uint64_t cache_version,
    bool has_cache_service_id,
    std::uint32_t cache_service_id,
    const std::vector<std::uint32_t> &cache_service_list,
    bool has_cache_sync_version,
    std::uint64_t cache_sync_version)
{
    const bool changed =
        lobby.has_cache_version != has_cache_version ||
        lobby.cache_version != cache_version ||
        lobby.has_cache_service_id != has_cache_service_id ||
        lobby.cache_service_id != cache_service_id ||
        lobby.cache_service_list != cache_service_list ||
        lobby.has_cache_sync_version != has_cache_sync_version ||
        lobby.cache_sync_version != cache_sync_version;
    lobby.has_cache_version = has_cache_version;
    lobby.cache_version = cache_version;
    lobby.has_cache_service_id = has_cache_service_id;
    lobby.cache_service_id = cache_service_id;
    lobby.cache_service_list = cache_service_list;
    lobby.has_cache_sync_version = has_cache_sync_version;
    lobby.cache_sync_version = cache_sync_version;
    return changed;
}

SteamAuthAckLaunchPlan compose_steam_auth_ack_launch_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t derived_ticket_crc)
{
    SteamAuthAckLaunchPlan plan{};
    plan.ticket_crc = current_lobby.launch_steam_auth_ticket_crc != 0u
        ? current_lobby.launch_steam_auth_ticket_crc
        : derived_ticket_crc;
    plan.message_sequence = current_lobby.launch_steam_auth_message_sequence != 0u
        ? current_lobby.launch_steam_auth_message_sequence
        : 1u;
    plan.mark_ack_queued = true;
    return plan;
}

void apply_steam_auth_ack_launch_plan(
    GBE_LocalLobby &lobby,
    const SteamAuthAckLaunchPlan &plan)
{
    lobby.launch_steam_auth_ticket_crc = plan.ticket_crc;
    lobby.launch_steam_auth_message_sequence = plan.message_sequence;
    lobby.launch_steam_auth_ack_queued = plan.mark_ack_queued;
}

bool mark_launch_4511_seen(GBE_LocalLobby &lobby)
{
    if (lobby.launch_4511_seen)
        return false;

    lobby.launch_4511_seen = true;
    return true;
}

bool restore_launch_4511_seen(
    GBE_LocalLobby &lobby,
    bool launch_4511_seen)
{
    if (lobby.launch_4511_seen == launch_4511_seen)
        return false;

    lobby.launch_4511_seen = launch_4511_seen;
    return true;
}

bool restore_lobby_owner_connected(
    GBE_LocalLobby &lobby,
    bool shared_owner_connected)
{
    return apply_lobby_owner_connected(lobby, shared_owner_connected);
}

bool apply_lobby_owner_connected(
    GBE_LocalLobby &lobby,
    bool owner_connected)
{
    if (lobby.owner_connected == owner_connected)
        return false;

    lobby.owner_connected = owner_connected;
    return true;
}

bool restore_lobby_owner_team(
    GBE_LocalLobby &lobby,
    std::uint32_t shared_owner_team)
{
    return apply_lobby_owner_team(lobby, shared_owner_team);
}

bool apply_lobby_owner_team(
    GBE_LocalLobby &lobby,
    std::uint32_t owner_team)
{
    if (lobby.owner_team == owner_team)
        return false;

    lobby.owner_team = owner_team;
    return true;
}

bool restore_lobby_owner_slot(
    GBE_LocalLobby &lobby,
    std::uint32_t shared_owner_slot)
{
    return apply_lobby_owner_slot(lobby, shared_owner_slot);
}

bool restore_lobby_members(
    GBE_LocalLobby &lobby,
    const std::vector<GBE_DotaLobbyMemberState> &shared_members)
{
    if (gbe::dota_lobby_flow::lobby_members_equal(lobby.members, shared_members))
        return false;
    lobby.members = shared_members;
    return true;
}

bool apply_lobby_owner_slot(
    GBE_LocalLobby &lobby,
    std::uint32_t owner_slot)
{
    if (lobby.owner_slot == owner_slot)
        return false;

    lobby.owner_slot = owner_slot;
    return true;
}

bool apply_lobby_owner_name(
    GBE_LocalLobby &lobby,
    const std::string &owner_name)
{
    if (lobby.owner_name == owner_name)
        return false;
    lobby.owner_name = owner_name;
    return true;
}

bool note_generic_lobby_local_member_seen(GBE_LocalLobby &lobby)
{
    const bool changed =
        !lobby.seen_local_in_generic_lobby ||
        lobby.kicked_suppressed_logged ||
        lobby.owner_adoption_suppressed_logged;
    lobby.seen_local_in_generic_lobby = true;
    lobby.kicked_suppressed_logged = false;
    lobby.owner_adoption_suppressed_logged = false;
    return changed;
}

bool mark_generic_lobby_waiting_join_confirmation_logged(GBE_LocalLobby &lobby)
{
    if (lobby.waiting_join_confirmation_logged)
        return false;
    lobby.waiting_join_confirmation_logged = true;
    return true;
}

bool mark_generic_lobby_kicked_suppressed_logged(GBE_LocalLobby &lobby)
{
    if (lobby.kicked_suppressed_logged)
        return false;
    lobby.kicked_suppressed_logged = true;
    return true;
}

bool mark_generic_lobby_owner_adoption_suppressed_logged(GBE_LocalLobby &lobby)
{
    if (lobby.owner_adoption_suppressed_logged)
        return false;
    lobby.owner_adoption_suppressed_logged = true;
    return true;
}

bool restore_lobby_custom_game(
    GBE_LocalLobby &lobby,
    const GBE_DotaCustomGameDetails &shared_custom_game)
{
    if (dota_custom_game::custom_game_details_equal(lobby.custom_game, shared_custom_game))
        return false;

    lobby.custom_game = shared_custom_game;
    return true;
}

bool apply_lobby_bot_difficulty_for_team(
    GBE_LocalLobby &lobby,
    std::uint32_t team,
    std::uint32_t bot_difficulty)
{
    std::uint32_t &target = proto_wire::dota_is_dire_team(team)
        ? lobby.bot_difficulty_dire
        : lobby.bot_difficulty_radiant;
    if (target == bot_difficulty)
        return false;
    target = bot_difficulty;
    return true;
}

bool apply_custom_game_loading_metadata(
    GBE_LocalLobby &lobby,
    std::uint64_t custom_game_id,
    std::uint32_t game_start_time)
{
    bool changed = false;
    if (custom_game_id != 0ull && lobby.custom_game.game_id != custom_game_id) {
        lobby.custom_game.game_id = custom_game_id;
        changed = true;
    }
    if (game_start_time != 0u && lobby.game_start_time != game_start_time) {
        lobby.game_start_time = game_start_time;
        changed = true;
    }
    return changed;
}

bool restore_lobby_generation(
    GBE_LocalLobby &lobby,
    std::uint64_t shared_generation)
{
    return apply_lobby_generation(lobby, shared_generation);
}

bool apply_lobby_generation(
    GBE_LocalLobby &lobby,
    std::uint64_t generation)
{
    if (lobby.generation == generation)
        return false;

    lobby.generation = generation;
    return true;
}

bool restore_lobby_generic_lobby_id(
    GBE_LocalLobby &lobby,
    std::uint64_t shared_generic_lobby_id)
{
    return apply_lobby_generic_lobby_id(lobby, shared_generic_lobby_id);
}

bool apply_lobby_generic_lobby_id(
    GBE_LocalLobby &lobby,
    std::uint64_t generic_lobby_id)
{
    if (lobby.generic_lobby_id == generic_lobby_id)
        return false;

    lobby.generic_lobby_id = generic_lobby_id;
    return true;
}

bool apply_source_tv_metadata(
    GBE_LocalLobby &lobby,
    std::uint64_t tv_secret_code,
    std::uint32_t tv_port)
{
    bool changed = false;
    if (tv_secret_code != 0 && lobby.tv_secret_code != tv_secret_code) {
        lobby.tv_secret_code = tv_secret_code;
        changed = true;
    }
    if (tv_port != 0 && lobby.tv_port != tv_port) {
        lobby.tv_port = tv_port;
        changed = true;
    }
    return changed;
}

bool apply_runtime_connect(
    GBE_LocalLobby &lobby,
    const std::string &connect)
{
    if (connect.empty() || lobby.connect == connect)
        return false;

    lobby.connect = connect;
    return true;
}

bool apply_runtime_metadata(
    GBE_LocalLobby &lobby,
    const std::string &connect,
    std::uint64_t server_id)
{
    bool changed = false;
    if (lobby.connect != connect) {
        lobby.connect = connect;
        changed = true;
    }
    changed = apply_lobby_server_id(lobby, server_id) || changed;
    return changed;
}

bool apply_lobby_server_id(
    GBE_LocalLobby &lobby,
    std::uint64_t server_id)
{
    if (lobby.server_id == server_id)
        return false;
    lobby.server_id = server_id;
    return true;
}

void apply_lobby_details_update(
    GBE_LocalLobby &lobby,
    const proto_wire::DotaPracticeLobbyDetailsRequest &details)
{
    apply_create_lobby_details(details, lobby);
}

bool apply_chat_channel(
    GBE_LocalLobby &lobby,
    std::uint64_t channel_id,
    const std::string &channel_name,
    std::uint32_t channel_type)
{
    const bool changed =
        !lobby.has_chat_channel ||
        lobby.chat_channel_id != channel_id ||
        lobby.chat_channel_name != channel_name ||
        lobby.chat_channel_type != channel_type;
    lobby.has_chat_channel = true;
    lobby.chat_channel_id = channel_id;
    lobby.chat_channel_name = channel_name;
    lobby.chat_channel_type = channel_type;
    return changed;
}

bool clear_chat_channel(GBE_LocalLobby &lobby)
{
    const bool changed =
        lobby.has_chat_channel ||
        lobby.chat_channel_id != 0ull ||
        !lobby.chat_channel_name.empty() ||
        lobby.chat_channel_type != 0u;
    lobby.has_chat_channel = false;
    lobby.chat_channel_id = 0ull;
    lobby.chat_channel_name.clear();
    lobby.chat_channel_type = 0u;
    return changed;
}

bool apply_broadcast_channel(
    GBE_LocalLobby &lobby,
    std::uint32_t channel_id,
    const std::string &country_code,
    const std::string &description,
    const std::string &language_code)
{
    const bool changed =
        !lobby.has_broadcast_channel ||
        lobby.broadcast_channel_id != channel_id ||
        lobby.broadcast_country_code != country_code ||
        lobby.broadcast_description != description ||
        lobby.broadcast_language_code != language_code;
    lobby.has_broadcast_channel = true;
    lobby.broadcast_channel_id = channel_id;
    lobby.broadcast_country_code = country_code;
    lobby.broadcast_description = description;
    lobby.broadcast_language_code = language_code;
    return changed;
}

bool patch_broadcast_channel(
    GBE_LocalLobby &lobby,
    std::uint32_t channel_id,
    bool has_country_code,
    const std::string &country_code,
    bool has_description,
    const std::string &description,
    bool has_language_code,
    const std::string &language_code)
{
    bool changed = !lobby.has_broadcast_channel || lobby.broadcast_channel_id != channel_id;
    lobby.has_broadcast_channel = true;
    lobby.broadcast_channel_id = channel_id;
    if (has_country_code && lobby.broadcast_country_code != country_code) {
        lobby.broadcast_country_code = country_code;
        changed = true;
    }
    if (has_description && lobby.broadcast_description != description) {
        lobby.broadcast_description = description;
        changed = true;
    }
    if (has_language_code && lobby.broadcast_language_code != language_code) {
        lobby.broadcast_language_code = language_code;
        changed = true;
    }
    return changed;
}

bool clear_broadcast_channel(
    GBE_LocalLobby &lobby,
    std::uint32_t channel_id)
{
    const bool changed =
        lobby.has_broadcast_channel ||
        lobby.broadcast_channel_id != channel_id ||
        !lobby.broadcast_country_code.empty() ||
        !lobby.broadcast_description.empty() ||
        !lobby.broadcast_language_code.empty();
    lobby.has_broadcast_channel = false;
    lobby.broadcast_channel_id = channel_id;
    lobby.broadcast_country_code.clear();
    lobby.broadcast_description.clear();
    lobby.broadcast_language_code.clear();
    return changed;
}

void apply_lifecycle_lobby_state(
    GBE_LocalLobby &lobby,
    std::uint32_t state,
    std::uint32_t game_state)
{
    lobby.state = state;
    lobby.game_state = game_state;
}

void apply_postgame_lobby_state_plan(
    GBE_LocalLobby &lobby,
    const PostGameLobbyStateApplyPlan &plan)
{
    lobby.state = plan.state;
    lobby.game_state = plan.game_state;
    lobby.has_chat_channel = true;
    lobby.chat_channel_id = plan.chat_channel_id;
    lobby.chat_channel_name = plan.chat_channel_name;
    lobby.chat_channel_type = 18u;
    lobby.abandon_pre_postgame_chat_channel_id = plan.abandon_pre_postgame_chat_channel_id;
    lobby.postgame_chat_tombstone_active = plan.abandon_pre_postgame_chat_channel_id != 0ull;
    lobby.postgame_chat_tombstone_channel_id = plan.abandon_pre_postgame_chat_channel_id;
    lobby.postgame_chat_tombstone_generation = lobby.postgame_chat_tombstone_active ? lobby.generation : 0ull;
    lobby.has_cache_version = false;
    lobby.cache_version = 0;
    lobby.has_cache_service_id = false;
    lobby.cache_service_id = 0;
    lobby.cache_service_list.clear();
    lobby.has_cache_sync_version = false;
    lobby.cache_sync_version = 0;
    lobby.abandon_postgame_active = true;
}

bool postgame_chat_tombstone_matches(
    const GBE_LocalLobby &lobby,
    std::uint64_t channel_id)
{
    return lobby.postgame_chat_tombstone_active &&
        channel_id != 0ull &&
        lobby.postgame_chat_tombstone_channel_id == channel_id &&
        lobby.postgame_chat_tombstone_generation == lobby.generation;
}

void clear_postgame_chat_tombstone(GBE_LocalLobby &lobby)
{
    lobby.abandon_pre_postgame_chat_channel_id = 0ull;
    lobby.postgame_chat_tombstone_active = false;
    lobby.postgame_chat_tombstone_channel_id = 0ull;
    lobby.postgame_chat_tombstone_generation = 0ull;
}

LaunchLifecycleTransitionDecision compute_custom_game_ready_up_transition(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t ready_state,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason)
{
    LaunchLifecycleTransitionDecision d{};
    if (!dota_custom_game::has_custom_game_details(current_lobby.custom_game))
        return d;
    if (ready_state != 1u || current_lobby.state != 2u || current_lobby.game_state >= 1u)
        return d;
    if (current_lobby.launch_phase < run_queued_launch_phase)
        return d;

    d.apply_lobby_state = true;
    d.next_state = current_lobby.state;
    d.next_game_state = 1u;
    d.publish_shared_state = true;
    d.send_details_update = true;
    d.reason = reason;
    return d;
}

LaunchLifecycleTransitionDecision compute_custom_game_started_loading_transition(
    const GBE_LocalLobby &current_lobby,
    bool matching_lobby,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason)
{
    LaunchLifecycleTransitionDecision d{};
    if (!matching_lobby || !dota_custom_game::has_custom_game_details(current_lobby.custom_game))
        return d;

    const LaunchRunPlan run_plan = compose_launch_run_plan(
        current_lobby,
        setup_synced_launch_phase,
        run_queued_launch_phase,
        0u);
    d.apply_lobby_state = run_plan.can_advance;
    d.next_state = run_plan.next_state;
    d.next_game_state = run_plan.next_game_state;
    d.mark_launch_phase = run_plan.can_advance;
    d.launch_phase = run_plan.launch_phase;
    d.queue_runtime_lobby_update = run_plan.can_advance;
    d.publish_shared_state = !run_plan.can_advance;
    d.send_details_update = !run_plan.can_advance;
    d.reason = reason;
    return d;
}

LaunchLifecycleTransitionDecision compute_custom_game_finished_loading_transition(
    const GBE_LocalLobby &current_lobby,
    bool matching_lobby,
    bool load_failed,
    std::uint32_t run_queued_launch_phase,
    std::uint32_t loaded_launch_phase,
    const std::string &reason)
{
    LaunchLifecycleTransitionDecision d{};
    if (!matching_lobby || !dota_custom_game::has_custom_game_details(current_lobby.custom_game))
        return d;

    d.apply_lobby_state = true;
    d.next_state = current_lobby.state;
    d.next_game_state = current_lobby.game_state;
    if (current_lobby.launch_phase >= run_queued_launch_phase) {
        d.next_state = 2u;
        if (d.next_game_state < 1u)
            d.next_game_state = 1u;
    } else if (d.next_state < 2u) {
        d.next_state = 2u;
    }

    d.mark_launch_phase = !load_failed;
    d.launch_phase = loaded_launch_phase;
    d.publish_shared_state = true;
    d.send_details_update = true;
    d.reason = reason;
    return d;
}

LaunchLifecycleTransitionDecision compute_runtime_game_state_transition(
    const GBE_LocalLobby &current_lobby,
    bool custom_game_launch,
    bool has_request_game_state,
    std::uint32_t request_game_state,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason)
{
    LaunchLifecycleTransitionDecision d{};
    if (!custom_game_launch || current_lobby.state != 2u)
        return d;
    if (current_lobby.launch_phase < run_queued_launch_phase)
        return d;
    if (!has_request_game_state || request_game_state <= current_lobby.game_state)
        return d;

    d.apply_lobby_state = true;
    d.next_state = 2u;
    d.next_game_state = request_game_state;
    d.queue_runtime_lobby_update = true;
    d.reason = reason;
    return d;
}

LaunchLifecycleTransitionDecision compute_launch_poll_transition(
    const GBE_LocalLobby &current_lobby,
    const std::string &reason)
{
    LaunchLifecycleTransitionDecision d{};
    if (current_lobby.state == 2u && current_lobby.game_state == 10u)
        return d;

    d.send_details_update = true;
    d.reason = reason;
    return d;
}

LaunchPresenceEvent compose_launch_serversetup_presence_event(const std::string &persona_reason)
{
    LaunchPresenceEvent event{};
    event.update = true;
    event.status = "#DOTA_RP_INIT";
    event.lobby_state = "SERVERSETUP";
    event.include_party = false;
    event.include_lobby = true;
    event.persona_reason = persona_reason;
    return event;
}

PracticeLobbyLaunchEventPlan compose_practice_lobby_launch_event_plan(std::uint32_t details_update_emsg)
{
    PracticeLobbyLaunchEventPlan plan{};
    plan.initial_details.send = true;
    plan.initial_details.emsg = details_update_emsg;
    plan.initial_details.reason = "7041_initial_26";
    plan.initial_details.apply_lobby_state = true;
    plan.initial_details.lobby_state = 1u;
    plan.initial_details.lobby_game_state = 0u;
    plan.initial_details.lobby_source = LaunchDetailsLobbySourceCurrent;
    plan.steam_auth_ack.queue = true;
    plan.steam_auth_ack.reason = "7041_serversetup";
    plan.presence = compose_launch_serversetup_presence_event("7041_launch_init");
    return plan;
}

CustomGameLaunchSetupEventPlan compose_custom_game_launch_setup_event_plan(std::uint32_t details_update_emsg)
{
    CustomGameLaunchSetupEventPlan plan{};
    LaunchDetailsEvent readyup{};
    readyup.send = true;
    readyup.emsg = details_update_emsg;
    readyup.reason = "7041_custom_game_readyup";
    readyup.apply_lobby_state = false;
    readyup.lobby_state = 4u;
    readyup.lobby_game_state = 0u;
    readyup.lobby_source = LaunchDetailsLobbySourceReadyUp;
    plan.details_events.push_back(readyup);

    LaunchDetailsEvent serversetup{};
    serversetup.send = true;
    serversetup.emsg = details_update_emsg;
    serversetup.reason = "7041_custom_game_serversetup";
    serversetup.apply_lobby_state = true;
    serversetup.lobby_state = 1u;
    serversetup.lobby_game_state = 0u;
    serversetup.lobby_source = LaunchDetailsLobbySourceServerSetup;
    plan.details_events.push_back(serversetup);

    plan.mark_phase_reason = "7041_custom_game_serversetup_synced";
    plan.steam_auth_ack.queue = true;
    plan.steam_auth_ack.reason = "7041_custom_game_serversetup";
    return plan;
}

bool should_preserve_known_owner_hero_on_publish(const GBE_LocalLobby &local, const GBE_SharedDotaLobbyState &shared)
{
    return local.owner_hero_id == 0u &&
        shared.valid &&
        shared.active &&
        local.active &&
        local.lobby_id != 0ull &&
        shared.lobby_id == local.lobby_id &&
        local.owner_steam_id != 0ull &&
        shared.owner_steam_id == local.owner_steam_id &&
        shared.owner_hero_id != 0u;
}

bool should_preserve_known_owner_hero_on_adopt(const GBE_LocalLobby &local, const GBE_SharedDotaLobbyState &shared)
{
    return local.active &&
        shared.active &&
        local.lobby_id != 0ull &&
        local.lobby_id == shared.lobby_id &&
        local.owner_steam_id != 0ull &&
        local.owner_steam_id == shared.owner_steam_id &&
        local.owner_hero_id != 0u &&
        shared.owner_hero_id == 0u;
}

bool should_peer_restore_owner_hero_from_client(
    bool server_role,
    const GBE_LocalLobby &server_local,
    const GBE_LocalLobby &client_local)
{
    if (!server_role)
        return false;
    if (server_local.owner_hero_id != 0u)
        return false;
    if (client_local.owner_hero_id == 0u)
        return false;
    if (!server_local.active || !client_local.active)
        return false;
    if (server_local.lobby_id == 0ull || client_local.lobby_id != server_local.lobby_id)
        return false;
    if (client_local.generation != server_local.generation)
        return false;
    if (server_local.owner_steam_id == 0ull || client_local.owner_steam_id != server_local.owner_steam_id)
        return false;
    return true;
}

bool apply_owner_hero_id(GBE_LocalLobby &local, std::uint32_t hero_id)
{
    if (hero_id == 0u || local.owner_hero_id == hero_id)
        return false;
    local.owner_hero_id = hero_id;
    return true;
}

bool apply_owner_hero_from_shared(GBE_LocalLobby &local, const GBE_SharedDotaLobbyState &shared)
{
    if (should_preserve_known_owner_hero_on_adopt(local, shared))
        return false;
    if (shared.owner_hero_id == 0u)
        return false;
    return apply_owner_hero_id(local, shared.owner_hero_id);
}

bool host_showcase_equip_key_matches(
    std::uint64_t key_generation,
    std::uint64_t key_lobby_id,
    std::uint64_t key_owner_steam_id,
    std::uint32_t key_owner_hero_id,
    std::uint64_t current_generation,
    std::uint64_t lobby_id,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_hero_id)
{
    return key_generation == current_generation &&
        key_lobby_id == lobby_id &&
        key_owner_steam_id == owner_steam_id &&
        key_owner_hero_id == owner_hero_id &&
        owner_hero_id != 0u;
}

bool host_wearable_refresh_key_matches(
    std::uint64_t key_generation,
    std::uint64_t key_steam_id,
    std::uint32_t key_hero_id,
    std::uint64_t current_generation,
    std::uint64_t steam_id,
    std::uint32_t hero_id)
{
    return key_generation == current_generation &&
        key_steam_id == steam_id &&
        key_hero_id == hero_id;
}

void publish_local_lobby_to_shared(const GBE_LocalLobby &local, bool is_server, GBE_SharedDotaLobbyState &shared)
{
    const bool preserve_known_owner_hero = should_preserve_known_owner_hero_on_publish(local, shared);
    shared.valid = true;
    shared.active = local.active;
    shared.generation = local.generation;
    shared.lobby_id = local.lobby_id;
    shared.generic_lobby_id = local.generic_lobby_id;
    shared.has_chat_channel = local.has_chat_channel;
    shared.chat_channel_id = local.chat_channel_id;
    shared.chat_channel_name = local.chat_channel_name;
    shared.chat_channel_type = local.chat_channel_type;
    shared.room_name = local.room_name;
    shared.game_mode = local.game_mode;
    shared.server_region = local.server_region;
    shared.lan = local.lan;
    shared.lan_host_ping_location = local.lan_host_ping_location;
    shared.allow_cheats = local.allow_cheats;
    shared.fill_with_bots = local.fill_with_bots;
    shared.allow_spectating = local.allow_spectating;
    shared.visibility = local.visibility;
    shared.bot_difficulty_radiant = local.bot_difficulty_radiant;
    shared.bot_difficulty_dire = local.bot_difficulty_dire;
    shared.bot_radiant = local.bot_radiant;
    shared.bot_dire = local.bot_dire;
    shared.custom_game = local.custom_game;
    if (is_server) {
        shared.state = local.state;
        shared.game_state = local.game_state;
        shared.server_id = local.server_id;
    } else {
        if (local.state > shared.state)
            shared.state = local.state;
        if (local.game_state > shared.game_state)
            shared.game_state = local.game_state;
        if (local.server_id != 0)
            shared.server_id = local.server_id;
    }
    shared.match_id = local.match_id;
    shared.owner_steam_id = local.owner_steam_id;
    shared.owner_account_id = local.owner_account_id;
    shared.owner_name = local.owner_name;
    shared.connect = local.connect;
    shared.game_start_time = local.game_start_time;
    shared.owner_team = local.owner_team;
    shared.owner_slot = local.owner_slot;
    if (!preserve_known_owner_hero)
        shared.owner_hero_id = local.owner_hero_id;
    shared.owner_connected = local.owner_connected;
    shared.members = local.members;
    shared.launch_phase = local.launch_phase;
    shared.launch_4511_seen = local.launch_4511_seen;
    shared.has_broadcast_channel = local.has_broadcast_channel;
    shared.broadcast_channel_id = local.broadcast_channel_id;
    shared.broadcast_country_code = local.broadcast_country_code;
    shared.broadcast_description = local.broadcast_description;
    shared.broadcast_language_code = local.broadcast_language_code;
    shared.pass_key = local.pass_key;
    shared.has_cache_version = local.has_cache_version;
    shared.cache_version = local.cache_version;
    shared.has_cache_service_id = local.has_cache_service_id;
    shared.cache_service_id = local.cache_service_id;
    shared.cache_service_list = local.cache_service_list;
    shared.has_cache_sync_version = local.has_cache_sync_version;
    shared.cache_sync_version = local.cache_sync_version;
}

void adopt_shared_lobby_to_local(
    const GBE_SharedDotaLobbyState &shared,
    bool clear_server_id_without_match,
    bool normalize_custom_readyup_run_state,
    GBE_LocalLobby &local)
{
    const bool preserve_known_owner_hero = should_preserve_known_owner_hero_on_adopt(local, shared);
    local.active = shared.active;
    local.generation = shared.generation;
    local.lobby_id = shared.lobby_id;
    local.generic_lobby_id = shared.generic_lobby_id;
    local.has_chat_channel = shared.has_chat_channel;
    local.chat_channel_id = shared.chat_channel_id;
    local.chat_channel_name = shared.chat_channel_name;
    local.chat_channel_type = shared.chat_channel_type;
    local.room_name = shared.room_name;
    local.game_mode = shared.game_mode;
    local.server_region = shared.server_region;
    local.lan = shared.lan;
    local.lan_host_ping_location = shared.lan_host_ping_location;
    local.allow_cheats = shared.allow_cheats;
    local.fill_with_bots = shared.fill_with_bots;
    local.allow_spectating = shared.allow_spectating;
    local.visibility = shared.visibility;
    local.bot_difficulty_radiant = shared.bot_difficulty_radiant;
    local.bot_difficulty_dire = shared.bot_difficulty_dire;
    local.bot_radiant = shared.bot_radiant;
    local.bot_dire = shared.bot_dire;
    local.custom_game = shared.custom_game;
    local.state = shared.state;
    local.game_state = shared.game_state;
    if (normalize_custom_readyup_run_state &&
        dota_custom_game::has_custom_game_details(local.custom_game) &&
        local.game_state >= 2u &&
        local.state == 4u)
        local.state = 2u;
    local.match_id = shared.match_id;
    local.server_id = clear_server_id_without_match && shared.match_id == 0ull ? 0ull : shared.server_id;
    local.owner_steam_id = shared.owner_steam_id;
    local.owner_account_id = shared.owner_account_id;
    local.owner_name = shared.owner_name;
    local.connect = proto_wire::normalize_dota_practice_lobby_connect(shared.connect);
    local.game_start_time = shared.game_start_time;
    local.owner_team = shared.owner_team;
    local.owner_slot = shared.owner_slot;
    // Full adopt may clear hero (shared==0). Runtime paths must use apply_owner_hero_id
    // (rejects 0). Incremental restore uses apply_owner_hero_from_shared instead.
    if (!preserve_known_owner_hero) {
        if (shared.owner_hero_id != 0u)
            apply_owner_hero_id(local, shared.owner_hero_id);
        else if (local.owner_hero_id != 0u)
            local.owner_hero_id = 0u;
    }
    local.owner_connected = shared.owner_connected;
    local.members = shared.members;
    local.launch_phase = shared.launch_phase;
    local.launch_4511_seen = shared.launch_4511_seen;
    local.has_broadcast_channel = shared.has_broadcast_channel;
    local.broadcast_channel_id = shared.broadcast_channel_id;
    local.broadcast_country_code = shared.broadcast_country_code;
    local.broadcast_description = shared.broadcast_description;
    local.broadcast_language_code = shared.broadcast_language_code;
    local.pass_key = shared.pass_key;
    local.has_cache_version = shared.has_cache_version;
    local.cache_version = shared.cache_version;
    local.has_cache_service_id = shared.has_cache_service_id;
    local.cache_service_id = shared.cache_service_id;
    local.cache_service_list = shared.cache_service_list;
    local.has_cache_sync_version = shared.has_cache_sync_version;
    local.cache_sync_version = shared.cache_sync_version;
}

bool build_dota_abandon_request_context(
    const GBE_LocalLobby &lobby,
    bool wrapped,
    bool has_wrapped_session,
    bool is_server,
    DotaAbandonRequestContext &context)
{
    context = {};
    if (!lobby.active || lobby.lobby_id == 0)
        return false;

    context.wrapped = wrapped;
    context.has_wrapped_session = has_wrapped_session;
    context.is_server = is_server;
    context.owner_connected = lobby.owner_connected;
    context.has_custom_game_details = dota_custom_game::has_custom_game_details(lobby.custom_game);
    context.lobby_id = lobby.lobby_id;
    context.lobby_state = lobby.state;
    context.game_state = lobby.game_state;
    context.server_id = lobby.server_id;
    context.launch_phase = lobby.launch_phase;
    context.pre_postgame_chat_channel_id = lobby.chat_channel_id;
    return true;
}

AbandonDecision compute_abandon_decision(const DotaAbandonRequestContext &context)
{
    AbandonDecision d;
    d.lobby_id = context.lobby_id;
    d.lobby_state = context.lobby_state;
    d.lobby_game_state = context.game_state;
    d.pre_postgame_chat_channel_id = context.pre_postgame_chat_channel_id;
    // Wrapped 7035 (user clicked Leave Game) can abandon at game_state >= 1
    // so players can leave during WAIT_FOR_PLAYERS_TO_LOAD if loading stalls.
    // Direct 7035 on a listen server (engine automatic state sync) requires
    // game_state >= 2 to avoid premature abandon during HERO_SELECTION.
    // Direct 7035 on a client (non-host) also uses game_state >= 1 because
    // the client has no engine-initiated 7035 -- it is always user-triggered.
    d.abandon_game_state_threshold = (context.wrapped || !context.is_server) ? 1u : 2u;
    d.treat_as_current_game_disconnect =
        context.is_server &&
        context.owner_connected &&
        d.lobby_state == 2u &&
        (context.server_id != 0 || d.lobby_game_state >= 1u);
    d.ready_for_abandon_teardown =
        d.lobby_state == 2u &&
        d.lobby_game_state >= d.abandon_game_state_threshold;
    d.arcade_launch_failed_before_connect =
        context.has_custom_game_details &&
        !context.wrapped &&
        !context.owner_connected &&
        d.lobby_state == 2u &&
        d.lobby_game_state >= 2u &&
        context.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
        context.launch_phase < GBE_kDotaLaunchPhaseLoaded;
    d.queue_cache_unsubscribed = d.arcade_launch_failed_before_connect ||
        (!d.ready_for_abandon_teardown && d.treat_as_current_game_disconnect);
    d.set_pending_reset_after_cache_unsubscribed = d.queue_cache_unsubscribed;
    d.discard_queued_launch_messages = d.arcade_launch_failed_before_connect || d.ready_for_abandon_teardown;
    d.suppress_abandoned_lobby = d.arcade_launch_failed_before_connect ||
        d.treat_as_current_game_disconnect ||
        d.ready_for_abandon_teardown;
    d.queue_postgame_teardown = d.ready_for_abandon_teardown && !d.arcade_launch_failed_before_connect;
    d.require_wrapped_session = context.wrapped && d.ready_for_abandon_teardown && !context.has_wrapped_session;
    d.suppress_previous_chat_channel = d.queue_postgame_teardown;
    d.push_postgame_cache_unsubscribed = d.queue_postgame_teardown;
    d.push_postgame_join = d.queue_postgame_teardown;
    return d;
}

AbandonDecision compute_abandon_decision(
    const GBE_LocalLobby &lobby,
    bool wrapped,
    bool is_server)
{
    DotaAbandonRequestContext context{};
    build_dota_abandon_request_context(lobby, wrapped, false, is_server, context);
    return compute_abandon_decision(context);
}

TeardownRetrievalDecision compute_teardown_retrieval_decision(
    bool is_dota_profile,
    bool pending_abandon_after_7014,
    bool pending_normal_signout_after_25,
    bool pending_reset_after_cache_unsubscribed,
    std::uint32_t retrieved_emsg,
    bool retrieved_other_left_matches_abandon_channel)
{
    TeardownRetrievalDecision d;
    if (!is_dota_profile)
        return d;

    d.finalize_abandon_after_7014 =
        pending_abandon_after_7014 &&
        retrieved_emsg == GBE_kDotaOtherLeftChannel &&
        retrieved_other_left_matches_abandon_channel;
    d.finalize_normal_signout_after_25 =
        pending_normal_signout_after_25 &&
        retrieved_emsg == GBE_kDotaCacheUnsubscribed;
    d.reset_after_cache_unsubscribed =
        pending_reset_after_cache_unsubscribed &&
        retrieved_emsg == GBE_kDotaCacheUnsubscribed;
    return d;
}

PostgameObservationDecision compute_postgame_observation_decision(
    bool is_server,
    bool host_has_active_server_gc,
    bool arcade_active_match,
    std::uint32_t previous_state,
    std::uint32_t current_state,
    std::uint64_t lobby_id)
{
    PostgameObservationDecision d;
    const bool transitioned_to_postgame = !is_server && previous_state < 3u && current_state >= 3u;
    d.skip_for_host_client = transitioned_to_postgame && host_has_active_server_gc;
    d.skip_for_arcade_active_match = transitioned_to_postgame && !host_has_active_server_gc && arcade_active_match;
    d.run_player_cleanup = transitioned_to_postgame &&
        !host_has_active_server_gc &&
        !arcade_active_match &&
        lobby_id != 0ull;
    return d;
}

RuntimeResetDecision compute_runtime_reset_decision(dota_diagnostic::Reason reason)
{
    RuntimeResetDecision d{};
    d.preserve_reconnect_context = reason == dota_diagnostic::Reason::DisconnectCurrentGameAfterCacheUnsubscribed;
    return d;
}

bool is_active_lobby_owned_by_local_user(
    const GBE_LocalLobby &lobby,
    std::uint64_t lobby_id,
    std::uint64_t local_steam_id)
{
    return lobby.active &&
        lobby.lobby_id == lobby_id &&
        local_steam_id != 0ull &&
        lobby.owner_steam_id == local_steam_id;
}

ReconnectEligibilityDecision compute_reconnect_eligibility_decision(
    bool source_valid,
    bool active,
    std::uint32_t lobby_state,
    std::uint32_t game_state,
    std::uint64_t server_id,
    bool has_connect,
    std::uint64_t custom_game_id,
    bool owner_connected,
    std::uint32_t launch_phase)
{
    ReconnectEligibilityDecision d;
    d.source_valid = source_valid;
    d.active = active;
    d.started = lobby_state >= 2u || game_state >= 2u;
    d.has_server_id = server_id != 0ull;
    d.has_connect = has_connect;
    d.custom_game = custom_game_id != 0ull;
    d.owner_connected = owner_connected;
    d.launch_run_or_later = launch_phase >= GBE_kDotaLaunchPhaseRunQueued;
    d.launch_loaded = launch_phase >= GBE_kDotaLaunchPhaseLoaded;
    d.context_eligible = d.source_valid && d.active && d.started && d.has_server_id && d.has_connect;
    return d;
}

ReconnectInterceptionDecision compute_reconnect_interception_decision(
    const GBE_DotaReconnectContext &context,
    bool has_context,
    std::uint64_t local_steam_id,
    std::uint64_t remote_steam_id,
    bool reconnect_eligible)
{
    ReconnectInterceptionDecision d;
    d.has_context = has_context;
    d.remote_matches_server = has_context && remote_steam_id == context.server_id;
    d.state_ready = has_context && GBE_DotaReconnectContextIsStarted(context);
    d.has_connect = has_context && context.connect[0] != '\0';
    d.arcade_context = has_context && context.custom_game_id != 0ull;
    d.local_is_owner = local_steam_id != 0ull && local_steam_id == context.owner_steam_id;
    d.reconnect_eligible = reconnect_eligible;
    d.p2p_rendezvous_candidate =
        d.remote_matches_server &&
        d.state_ready &&
        d.has_connect &&
        !d.local_is_owner;
    d.can_post_connection_state =
        d.has_context &&
        d.arcade_context &&
        d.state_ready &&
        d.has_connect &&
        !d.local_is_owner &&
        d.reconnect_eligible;
    return d;
}

} // namespace gbe::dota_lobby_state
