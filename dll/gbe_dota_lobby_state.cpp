#include "gbe_dota_lobby_state.h"

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

void publish_local_lobby_to_shared(const GBE_LocalLobby &local, bool is_server, GBE_SharedDotaLobbyState &shared)
{
    shared.valid = true;
    shared.active = local.active;
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
    local.active = shared.active;
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
    local.owner_hero_id = shared.owner_hero_id;
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

RuntimeResetDecision compute_runtime_reset_decision(const char *reason)
{
    RuntimeResetDecision d{};
    d.preserve_reconnect_context = reason && std::strcmp(reason, "7035_disconnect_current_game_after_25") == 0;
    return d;
}

bool build_reconnect_context(const GBE_LocalLobby &local, GBE_DotaReconnectContext &context)
{
    context = GBE_DotaReconnectContext{};
    const auto eligibility = compute_reconnect_eligibility_decision(
        true,
        local.active,
        local.state,
        local.game_state,
        local.server_id,
        !local.connect.empty(),
        local.custom_game.game_id,
        local.owner_connected,
        local.launch_phase);
    if (!eligibility.context_eligible)
        return false;

    context.server_id = local.server_id;
    context.lobby_state = local.state;
    context.game_state = local.game_state;
    context.custom_game_id = local.custom_game.game_id;
    const std::string endpoint = proto_wire::get_dota_practice_lobby_first_connect_endpoint(local.connect);
    std::strncpy(context.connect, endpoint.c_str(), sizeof(context.connect) - 1);
    context.connect[sizeof(context.connect) - 1] = '\0';
    context.owner_steam_id = local.owner_steam_id;
    return true;
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
