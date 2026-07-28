// Focused tests for gbe::dota_lobby_state transition helpers.
//
// Covers the four Phase 3.4 transition categories:
//   1. Valid launch progression (compose_launch_init_plan, has_launch_server_setup_sync,
//      compose_launch_run_plan, compose_queued_lobby_state_apply_plan).
//   2. Stale generic lobby state regression (adopt_shared_lobby_to_local readyup normalization,
//      compose_queued_lobby_state_apply_plan monotonic game_state preservation).
//   3. Owner disconnect and reconnect (compute_abandon_decision treat_as_current_game_disconnect,
//      build_reconnect_context).
//   4. Post-game teardown suppression (compute_abandon_decision ready_for_abandon_teardown,
//      arcade_launch_failed_before_connect, threshold).

#include "dll/gbe_dota_lobby_state.h"
#include "dll/gbe_dota_lifecycle_actions.h"
#include "dll/gbe_dota_lobby_flow.h"
#include "dll/gbe_dota_lobby_generation.h"
#include "dll/gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_context.h"
#include "dll/gbe_dota_types.h"
#include "dll/dll/gbe_dota_reconnect_shared.h"
#include "dll/dll/gbe_dota_serialized_connection_state.h"
#include "dll/gbe_dota_protocol_constants.h"

#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;
    std::cerr << "failed: " << label << std::endl;
    return false;
}

std::string lifecycle_action_fingerprint(const GBE_DotaActionList &actions)
{
    std::ostringstream fingerprint;
    for (const GBE_DotaAction &action : actions) {
        fingerprint
            << static_cast<unsigned int>(action.type) << '|'
            << action.emsg << '|'
            << action.payload << '|'
            << action.target_steam_id << '|'
            << action.item_id << '|'
            << action.job_id << '|'
            << action.reason << '|'
            << action.leave_generic_lobby << '|'
            << action.clear_queued_messages << '|'
            << action.lobby_state << '|'
            << action.lobby_game_state << '|'
            << action.launch_phase << '|'
            << action.hero_id << '|'
            << action.connected << '|'
            << action.has_hero_id << '|'
            << action.only_when_previous_action_succeeded << '|'
            << action.only_when_runtime_update_not_queued << '|'
            << action.delay << ';';
    }
    return fingerprint.str();
}

bool expect_false(bool value, const char *label)
{
    if (!value)
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

bool expect_eq_u32(std::uint32_t actual, std::uint32_t expected, const char *label)
{
    if (actual == expected)
        return true;
    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool expect_eq_str(const std::string &actual, const std::string &expected, const char *label)
{
    if (actual == expected)
        return true;
    std::cerr << "failed: " << label << " actual=\"" << actual << "\" expected=\"" << expected << "\"" << std::endl;
    return false;
}

GBE_LocalLobby make_active_lobby()
{
    GBE_LocalLobby lobby{};
    lobby.active = true;
    lobby.lobby_id = 100ull;
    return lobby;
}

bool test_local_lobby_owner_boundaries()
{
    bool ok = true;

    GBE_LocalLobby backing = make_active_lobby();
    backing.generation = 7ull;
    gbe::dota_lobby_state::LocalLobbyOwner owner(backing);

    ok &= expect_eq_u64(owner.snapshot().lobby_id, 100ull, "owner snapshot reads backing lobby");
    ok &= expect_eq_u64(owner.snapshot().generation, 7ull, "owner snapshot reads generation");

    GBE_LocalLobby reset_lobby{};
    reset_lobby.active = true;
    reset_lobby.lobby_id = 200ull;
    reset_lobby.generation = 8ull;
    owner.replace_for_reset(reset_lobby);
    ok &= expect_eq_u64(backing.lobby_id, 200ull, "owner reset replaces backing lobby");
    ok &= expect_eq_u64(owner.snapshot().generation, 8ull, "owner reset updates snapshot");

    owner.apply("unit_test_apply", [](GBE_LocalLobby &lobby) {
        lobby.lobby_id = 300ull;
        lobby.state = 2u;
        return true;
    });
    ok &= expect_eq_u64(backing.lobby_id, 300ull, "owner apply mutates through named boundary");
    ok &= expect_eq_u32(owner.snapshot().state, 2u, "owner apply preserves mutation result");

    return ok;
}

// ---- Category 1: valid launch progression ---------------------------------

bool test_valid_launch_progression()
{
    bool ok = true;

    // compose_launch_init_plan: populates match_id/server_id/connect/game_start_time/launch_phase.
    {
        GBE_LocalLobby current = make_active_lobby();
        current.connect = "";  // empty -> candidate preferred
        auto plan = gbe::dota_lobby_state::compose_launch_init_plan(
            current, 500ull, 700ull, "1.2.3.4:27015", 11111u, GBE_kDotaLaunchPhaseRequested);
        ok &= expect_eq_u64(plan.lobby.match_id, 500ull, "launch_init match_id");
        ok &= expect_eq_u64(plan.lobby.server_id, 700ull, "launch_init server_id");
        ok &= expect_eq_str(plan.lobby.connect, "1.2.3.4:27015", "launch_init connect preferred when empty");
        ok &= expect_eq_u32(plan.lobby.game_start_time, 11111u, "launch_init game_start_time");
        ok &= expect_eq_u32(plan.lobby.launch_phase, GBE_kDotaLaunchPhaseRequested, "launch_init launch_phase");
        GBE_LocalLobby applied = make_active_lobby();
        applied.lobby_id = 321ull;
        gbe::dota_lobby_state::apply_launch_init_plan(applied, plan);
        ok &= expect_eq_u64(applied.match_id, 500ull, "launch_init apply updates match_id");
        ok &= expect_eq_u64(applied.server_id, 700ull, "launch_init apply updates server_id");
        ok &= expect_eq_str(applied.connect, "1.2.3.4:27015", "launch_init apply updates connect");
        ok &= expect_eq_u32(applied.launch_phase, GBE_kDotaLaunchPhaseRequested, "launch_init apply updates launch phase");
        auto custom_setup_plan = gbe::dota_lobby_state::compose_custom_game_launch_setup_plan(
            plan.lobby, GBE_kDotaLaunchPhaseSetupSynced);
        ok &= expect_eq_u32(custom_setup_plan.readyup_lobby.state, 4u, "custom setup readyup state");
        ok &= expect_eq_u32(custom_setup_plan.serversetup_lobby.state, 1u, "custom setup serversetup state");
        gbe::dota_lobby_state::apply_custom_game_launch_serversetup_plan(applied, custom_setup_plan);
        ok &= expect_eq_u64(applied.match_id, 500ull, "custom setup apply preserves match_id");
        ok &= expect_eq_u32(applied.state, 1u, "custom setup apply updates serversetup state");
        ok &= expect_eq_u32(applied.game_state, 0u, "custom setup apply updates serversetup game_state");
        GBE_LocalLobby restore_target = make_active_lobby();
        restore_target.lobby_id = 444ull;
        gbe::dota_lobby_state::apply_client_lobby_restore_snapshot(restore_target, applied);
        ok &= expect_eq_u64(restore_target.lobby_id, applied.lobby_id, "client restore apply replaces Local snapshot");
        ok &= expect_eq_u64(restore_target.match_id, 500ull, "client restore apply preserves match_id");
        ok &= expect_eq_u32(restore_target.state, 1u, "client restore apply preserves state");
    }

    // compose_launch_init_plan: does NOT overwrite a non-loopback connect with a different one.
    {
        GBE_LocalLobby current = make_active_lobby();
        current.connect = "5.6.7.8:27015";  // non-loopback, different from candidate
        auto plan = gbe::dota_lobby_state::compose_launch_init_plan(
            current, 500ull, 700ull, "1.2.3.4:27015", 11111u, GBE_kDotaLaunchPhaseRequested);
        ok &= expect_eq_str(plan.lobby.connect, "5.6.7.8:27015", "launch_init connect kept when non-loopback");
    }

    // has_launch_server_setup_sync: requires active + lobby_id + match_id + game_start_time + connect.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.match_id = 1ull;
        lobby.game_start_time = 1u;
        lobby.connect = "1.2.3.4:27015";
        ok &= expect_true(gbe::dota_lobby_state::has_launch_server_setup_sync(lobby), "sync true when fully provisioned");

        lobby.active = false;
        ok &= expect_false(gbe::dota_lobby_state::has_launch_server_setup_sync(lobby), "sync false when inactive");
        lobby.active = true;

        lobby.lobby_id = 0ull;
        ok &= expect_false(gbe::dota_lobby_state::has_launch_server_setup_sync(lobby), "sync false when lobby_id zero");
        lobby.lobby_id = 100ull;

        lobby.match_id = 0ull;
        ok &= expect_false(gbe::dota_lobby_state::has_launch_server_setup_sync(lobby), "sync false when match_id zero");
        lobby.match_id = 1ull;

        lobby.game_start_time = 0u;
        ok &= expect_false(gbe::dota_lobby_state::has_launch_server_setup_sync(lobby), "sync false when game_start_time zero");
        lobby.game_start_time = 1u;

        lobby.connect.clear();
        ok &= expect_false(gbe::dota_lobby_state::has_launch_server_setup_sync(lobby), "sync false when connect empty");
    }

    // compose_launch_run_plan: blocked when server setup sync not established.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        // match_id=0 -> no sync
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
        auto plan = gbe::dota_lobby_state::compose_launch_run_plan(
            lobby, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued, 2u);
        ok &= expect_false(plan.can_advance, "run_plan blocked when no server setup sync");
    }

    // compose_launch_run_plan: blocked when launch_phase < setup_synced.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.match_id = 1ull;
        lobby.game_start_time = 1u;
        lobby.connect = "1.2.3.4:27015";
        lobby.launch_phase = GBE_kDotaLaunchPhaseRequested;  // < setup_synced
        auto plan = gbe::dota_lobby_state::compose_launch_run_plan(
            lobby, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued, 2u);
        ok &= expect_false(plan.can_advance, "run_plan blocked when launch_phase below setup_synced");
    }

    // compose_launch_run_plan: advances and bumps phase to run_queued when current is below.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.match_id = 1ull;
        lobby.game_start_time = 1u;
        lobby.connect = "1.2.3.4:27015";
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;  // >= setup_synced, < run_queued
        auto plan = gbe::dota_lobby_state::compose_launch_run_plan(
            lobby, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued, 2u);
        ok &= expect_true(plan.can_advance, "run_plan advances when sync established");
        ok &= expect_eq_u32(plan.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "run_plan bumps phase to run_queued");
        ok &= expect_eq_u32(plan.next_state, 2u, "run_plan next_state is RUN");
        ok &= expect_eq_u32(plan.next_game_state, 2u, "run_plan next_game_state");
    }

    // compose_launch_run_plan: keeps current phase when already at or above run_queued.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.match_id = 1ull;
        lobby.game_start_time = 1u;
        lobby.connect = "1.2.3.4:27015";
        lobby.launch_phase = GBE_kDotaLaunchPhaseLoaded;  // > run_queued
        auto plan = gbe::dota_lobby_state::compose_launch_run_plan(
            lobby, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued, 2u);
        ok &= expect_true(plan.can_advance, "run_plan advances when already loaded");
        ok &= expect_eq_u32(plan.launch_phase, GBE_kDotaLaunchPhaseLoaded, "run_plan keeps loaded phase");
    }

    // advance_launch_phase: update once and preserve monotonic launch progress.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
        ok &= expect_true(
            gbe::dota_lobby_state::advance_launch_phase(lobby, GBE_kDotaLaunchPhaseRunQueued),
            "launch phase advances to run queued");
        ok &= expect_eq_u32(lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "launch phase applies next value");
        ok &= expect_false(
            gbe::dota_lobby_state::advance_launch_phase(lobby, GBE_kDotaLaunchPhaseSetupSynced),
            "launch phase rejects regression");
        ok &= expect_eq_u32(lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "launch phase keeps monotonic value");
    }

    // generic lobby capture: preserve custom-game launch progress and apply fresh fields together.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 1ull;
        lobby.match_id = 2ull;
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
        lobby.state = 2u;
        lobby.game_state = 3u;
        const auto stale_plan = gbe::dota_lobby_state::compose_generic_lobby_state_capture_plan(
            lobby, true, 1u, true, 2u, GBE_kDotaLaunchPhaseSetupSynced);
        ok &= expect_true(stale_plan.ignored_stale_state, "generic capture identifies stale state");
        ok &= expect_false(stale_plan.apply_state, "generic capture skips stale state");
        ok &= expect_false(stale_plan.apply_game_state, "generic capture skips stale game state");
        gbe::dota_lobby_state::apply_generic_lobby_state_capture_plan(lobby, stale_plan);
        ok &= expect_eq_u32(lobby.state, 2u, "generic capture preserves state");
        ok &= expect_eq_u32(lobby.game_state, 3u, "generic capture preserves game_state");

        const auto fresh_plan = gbe::dota_lobby_state::compose_generic_lobby_state_capture_plan(
            lobby, true, 3u, true, 4u, GBE_kDotaLaunchPhaseSetupSynced);
        gbe::dota_lobby_state::apply_generic_lobby_state_capture_plan(lobby, fresh_plan);
        ok &= expect_eq_u32(lobby.state, 3u, "generic capture applies state");
        ok &= expect_eq_u32(lobby.game_state, 4u, "generic capture applies game_state");
    }

    // generic runtime identity capture: room/match/server/connect/start_time with LAN preserve.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.room_name = "alpha";
        lobby.match_id = 100ull;
        lobby.server_id = 200ull;
        lobby.connect = "1.2.3.4:27015";
        lobby.game_start_time = 10u;
        const auto open_plan = gbe::dota_lobby_state::compose_generic_lobby_runtime_identity_capture_plan(
            lobby,
            "beta",
            "300",
            "400",
            "5.6.7.8:27015",
            "20");
        ok &= expect_true(open_plan.apply_room_name, "generic identity applies room");
        ok &= expect_true(open_plan.apply_match_id, "generic identity applies match");
        ok &= expect_true(open_plan.apply_server_id, "generic identity applies server");
        ok &= expect_true(open_plan.apply_connect, "generic identity applies connect");
        ok &= expect_true(open_plan.apply_game_start_time, "generic identity applies start time");
        gbe::dota_lobby_state::apply_generic_lobby_runtime_identity_capture_plan(lobby, open_plan);
        ok &= expect_true(lobby.room_name == "beta", "generic identity updates room");
        ok &= expect_eq_u64(lobby.match_id, 300ull, "generic identity updates match");
        ok &= expect_eq_u64(lobby.server_id, 400ull, "generic identity updates server");
        ok &= expect_true(lobby.connect == "5.6.7.8:27015", "generic identity updates connect");
        ok &= expect_eq_u32(lobby.game_start_time, 20u, "generic identity updates start time");

        lobby = make_active_lobby();
        lobby.custom_game.game_id = 0ull;
        lobby.lan = true;
        lobby.match_id = 100ull;
        lobby.server_id = 200ull;
        lobby.connect = "1.2.3.4:27015";
        const auto preserve_plan = gbe::dota_lobby_state::compose_generic_lobby_runtime_identity_capture_plan(
            lobby,
            "",
            "0",
            "999",
            "9.9.9.9:27015",
            "");
        ok &= expect_false(preserve_plan.apply_match_id, "generic identity rejects zero match when local match exists");
        ok &= expect_true(preserve_plan.preserve_existing_lan_runtime, "generic identity preserves launched LAN runtime");
        ok &= expect_false(preserve_plan.apply_server_id, "generic identity keeps LAN server_id");
        ok &= expect_false(preserve_plan.apply_connect, "generic identity keeps LAN connect");
        gbe::dota_lobby_state::apply_generic_lobby_runtime_identity_capture_plan(lobby, preserve_plan);
        ok &= expect_eq_u64(lobby.match_id, 100ull, "generic identity preserves local match");
        ok &= expect_eq_u64(lobby.server_id, 200ull, "generic identity preserves local server");
        ok &= expect_true(lobby.connect == "1.2.3.4:27015", "generic identity preserves local connect");
    }

    // generic options capture: present raw keys update options/bot fields.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.allow_cheats = false;
        lobby.fill_with_bots = false;
        lobby.allow_spectating = true;
        lobby.visibility = 1u;
        lobby.bot_difficulty_radiant = 1u;
        lobby.bot_difficulty_dire = 1u;
        lobby.bot_radiant = 1ull;
        lobby.bot_dire = 1ull;
        const auto empty_plan = gbe::dota_lobby_state::compose_generic_lobby_options_capture_plan(
            "", "", "", "", "", "", "", "");
        ok &= expect_false(empty_plan.apply_allow_cheats, "generic options skips empty cheats");
        ok &= expect_false(empty_plan.apply_bot_dire, "generic options skips empty bot dire");
        gbe::dota_lobby_state::apply_generic_lobby_options_capture_plan(lobby, empty_plan);
        ok &= expect_false(lobby.allow_cheats, "generic options keeps cheats when raw empty");
        ok &= expect_eq_u64(lobby.bot_dire, 1ull, "generic options keeps bot dire when raw empty");
        const auto present_plan = gbe::dota_lobby_state::compose_generic_lobby_options_capture_plan(
            "1", "1", "0", "2", "3", "4", "5", "6");
        ok &= expect_true(present_plan.apply_allow_cheats, "generic options applies cheats");
        ok &= expect_true(present_plan.apply_fill_with_bots, "generic options applies fill bots");
        ok &= expect_true(present_plan.apply_allow_spectating, "generic options applies spectating");
        ok &= expect_true(present_plan.apply_visibility, "generic options applies visibility");
        ok &= expect_true(present_plan.apply_bot_difficulty_radiant, "generic options applies radiant difficulty");
        ok &= expect_true(present_plan.apply_bot_difficulty_dire, "generic options applies dire difficulty");
        ok &= expect_true(present_plan.apply_bot_radiant, "generic options applies radiant bots");
        ok &= expect_true(present_plan.apply_bot_dire, "generic options applies dire bots");
        gbe::dota_lobby_state::apply_generic_lobby_options_capture_plan(lobby, present_plan);
        ok &= expect_true(lobby.allow_cheats, "generic options updates cheats");
        ok &= expect_true(lobby.fill_with_bots, "generic options updates fill bots");
        ok &= expect_false(lobby.allow_spectating, "generic options updates spectating");
        ok &= expect_eq_u32(lobby.visibility, 2u, "generic options updates visibility");
        ok &= expect_eq_u32(lobby.bot_difficulty_radiant, 3u, "generic options updates radiant difficulty");
        ok &= expect_eq_u32(lobby.bot_difficulty_dire, 4u, "generic options updates dire difficulty");
        ok &= expect_eq_u64(lobby.bot_radiant, 5ull, "generic options updates radiant bots");
        ok &= expect_eq_u64(lobby.bot_dire, 6ull, "generic options updates dire bots");
    }

    // generic custom game capture: present raw keys update custom_game fields.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.mode = "old_mode";
        lobby.custom_game.map_name = "old_map";
        lobby.custom_game.difficulty = 1u;
        lobby.custom_game.game_id = 11ull;
        lobby.custom_game.min_players = 2u;
        lobby.custom_game.max_players = 4u;
        lobby.custom_game.crc = 5ull;
        lobby.custom_game.timestamp = 6u;
        lobby.custom_game.penalties = false;
        const auto empty_plan = gbe::dota_lobby_state::compose_generic_lobby_custom_game_capture_plan(
            "", "", "", "", "", "", "", "", "");
        ok &= expect_false(empty_plan.apply_mode, "generic custom game skips empty mode");
        ok &= expect_false(empty_plan.apply_game_id, "generic custom game skips empty game id");
        gbe::dota_lobby_state::apply_generic_lobby_custom_game_capture_plan(lobby, empty_plan);
        ok &= expect_true(lobby.custom_game.mode == "old_mode", "generic custom game keeps mode when raw empty");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 11ull, "generic custom game keeps game id when raw empty");
        const auto present_plan = gbe::dota_lobby_state::compose_generic_lobby_custom_game_capture_plan(
            "new_mode",
            "new_map",
            "3",
            "22",
            "5",
            "10",
            "99",
            "100",
            "1");
        ok &= expect_true(present_plan.apply_mode, "generic custom game applies mode");
        ok &= expect_true(present_plan.apply_map_name, "generic custom game applies map");
        ok &= expect_true(present_plan.apply_difficulty, "generic custom game applies difficulty");
        ok &= expect_true(present_plan.apply_game_id, "generic custom game applies game id");
        ok &= expect_true(present_plan.apply_min_players, "generic custom game applies min players");
        ok &= expect_true(present_plan.apply_max_players, "generic custom game applies max players");
        ok &= expect_true(present_plan.apply_crc, "generic custom game applies crc");
        ok &= expect_true(present_plan.apply_timestamp, "generic custom game applies timestamp");
        ok &= expect_true(present_plan.apply_penalties, "generic custom game applies penalties");
        gbe::dota_lobby_state::apply_generic_lobby_custom_game_capture_plan(lobby, present_plan);
        ok &= expect_true(lobby.custom_game.mode == "new_mode", "generic custom game updates mode");
        ok &= expect_true(lobby.custom_game.map_name == "new_map", "generic custom game updates map");
        ok &= expect_eq_u32(lobby.custom_game.difficulty, 3u, "generic custom game updates difficulty");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 22ull, "generic custom game updates game id");
        ok &= expect_eq_u32(lobby.custom_game.min_players, 5u, "generic custom game updates min players");
        ok &= expect_eq_u32(lobby.custom_game.max_players, 10u, "generic custom game updates max players");
        ok &= expect_eq_u64(lobby.custom_game.crc, 99ull, "generic custom game updates crc");
        ok &= expect_eq_u32(lobby.custom_game.timestamp, 100u, "generic custom game updates timestamp");
        ok &= expect_true(lobby.custom_game.penalties, "generic custom game updates penalties");
    }

    // generic capture: one plan applies state, runtime, options, and custom-game fields.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        gbe::dota_lobby_state::GenericLobbyCaptureInput input{};
        input.has_state = true;
        input.state = 3u;
        input.has_game_state = true;
        input.game_state = 4u;
        input.room_name = "capture_room";
        input.match_id_raw = "10";
        input.server_id_raw = "20";
        input.connect = "10.0.0.1:27015";
        input.game_start_time_raw = "30";
        input.allow_cheats_raw = "1";
        input.fill_with_bots_raw = "1";
        input.allow_spectating_raw = "0";
        input.visibility_raw = "2";
        input.bot_difficulty_radiant_raw = "3";
        input.bot_difficulty_dire_raw = "4";
        input.bot_radiant_raw = "5";
        input.bot_dire_raw = "6";
        input.custom_game_mode = "capture_mode";
        input.custom_map_name = "capture_map";
        input.custom_difficulty_raw = "7";
        input.custom_game_id_raw = "8";
        input.custom_min_players_raw = "9";
        input.custom_max_players_raw = "10";
        input.custom_game_crc_raw = "11";
        input.custom_game_timestamp_raw = "12";
        input.custom_game_penalties_raw = "1";
        const auto plan = gbe::dota_lobby_state::compose_generic_lobby_capture_plan(
            lobby,
            input,
            GBE_kDotaLaunchPhaseSetupSynced);
        ok &= expect_true(plan.state.apply_state, "generic capture aggregates state");
        ok &= expect_true(plan.runtime_identity.apply_connect, "generic capture aggregates runtime");
        ok &= expect_true(plan.options.apply_allow_cheats, "generic capture aggregates options");
        ok &= expect_true(plan.custom_game.apply_game_id, "generic capture aggregates custom game");
        gbe::dota_lobby_state::apply_generic_lobby_capture_plan(lobby, plan);
        ok &= expect_eq_u32(lobby.state, 3u, "generic capture applies state");
        ok &= expect_eq_u32(lobby.game_state, 4u, "generic capture applies game state");
        ok &= expect_true(lobby.room_name == "capture_room", "generic capture applies room");
        ok &= expect_eq_u64(lobby.match_id, 10ull, "generic capture applies match");
        ok &= expect_true(lobby.connect == "10.0.0.1:27015", "generic capture applies connect");
        ok &= expect_true(lobby.allow_cheats, "generic capture applies cheats");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 8ull, "generic capture applies custom game id");
    }

    // Snapshot projection applies the generic plan to a copy and retains the Local source.
    {
        GBE_LocalLobby local_lobby = make_active_lobby();
        local_lobby.state = 1u;
        local_lobby.room_name = "local_room";
        local_lobby.custom_game.game_id = 4ull;
        const GBE_LocalLobby local_before_projection = local_lobby;
        gbe::dota_lobby_state::GenericLobbyCaptureInput input{};
        input.has_state = true;
        input.state = 3u;
        input.room_name = "projected_room";
        input.custom_game_id_raw = "8";
        const auto plan = gbe::dota_lobby_state::compose_generic_lobby_capture_plan(
            local_lobby,
            input,
            GBE_kDotaLaunchPhaseSetupSynced);
        GBE_LocalLobby snapshot = local_lobby;
        gbe::dota_lobby_state::apply_generic_lobby_capture_plan(snapshot, plan);
        ok &= expect_eq_u32(snapshot.state, 3u, "snapshot projection applies generic state");
        ok &= expect_true(snapshot.room_name == "projected_room", "snapshot projection applies generic room");
        ok &= expect_eq_u64(snapshot.custom_game.game_id, 8ull, "snapshot projection applies generic custom game");
        ok &= expect_eq_u32(local_lobby.state, local_before_projection.state, "snapshot projection retains Local state");
        ok &= expect_true(local_lobby.room_name == local_before_projection.room_name, "snapshot projection retains Local room");
        ok &= expect_eq_u64(local_lobby.custom_game.game_id, local_before_projection.custom_game.game_id, "snapshot projection retains Local custom game");
    }

    // shared runtime restore: retain local RUN when a custom-game READYUP snapshot regresses state.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 1ull;
        lobby.match_id = 2ull;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        lobby.state = 2u;
        lobby.game_state = 3u;
        GBE_SharedDotaLobbyState shared{};
        shared.state = 4u;
        shared.game_state = 2u;
        shared.launch_phase = GBE_kDotaLaunchPhaseLoaded;
        const auto regression_plan = gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
            lobby, shared, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_true(regression_plan.ignored_readyup_regression, "shared restore identifies READYUP regression");
        ok &= expect_false(regression_plan.apply_state, "shared restore keeps RUN state");
        ok &= expect_true(regression_plan.apply_game_state, "shared restore applies shared game_state");
        ok &= expect_true(regression_plan.apply_launch_phase, "shared restore applies shared launch phase");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_source_aware_shared_runtime_restore_plan(lobby, regression_plan),
            "shared restore reports runtime change");
        ok &= expect_eq_u32(lobby.state, 2u, "shared restore retains RUN state");
        ok &= expect_eq_u32(lobby.game_state, 2u, "shared restore updates game_state");
        ok &= expect_eq_u32(lobby.launch_phase, GBE_kDotaLaunchPhaseLoaded, "shared restore updates launch phase");
    }

    // Same-generation generic capture keeps its Local launch/runtime field groups over an older shared snapshot.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 9ull;
        gbe::dota_lobby_state::GenericLobbyCaptureInput input{};
        input.has_state = true;
        input.state = 3u;
        input.has_game_state = true;
        input.game_state = 4u;
        input.room_name = "local-room";
        input.match_id_raw = "22";
        input.server_id_raw = "33";
        input.connect = "new-connect";
        input.game_start_time_raw = "44";
        const auto capture_plan = gbe::dota_lobby_state::compose_generic_lobby_capture_plan(
            lobby, input, GBE_kDotaLaunchPhaseSetupSynced);
        gbe::dota_lobby_state::apply_generic_lobby_capture_plan(lobby, capture_plan);
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation;
        shared.state = 1u;
        shared.game_state = 2u;
        shared.launch_phase = GBE_kDotaLaunchPhaseLoaded;
        shared.room_name = "shared-room";
        shared.match_id = 11ull;
        shared.server_id = 12ull;
        shared.connect = "old-connect";
        shared.game_start_time = 13u;
        const auto restore_plan = gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
            lobby, shared, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_false(restore_plan.apply_state, "same-generation capture keeps local state");
        ok &= expect_false(restore_plan.apply_game_state, "same-generation capture keeps local game state");
        ok &= expect_false(restore_plan.apply_launch_phase, "same-generation capture keeps local launch phase");
        ok &= expect_false(restore_plan.apply_room_name, "same-generation capture keeps local room");
        ok &= expect_false(restore_plan.apply_connect, "same-generation capture keeps local connect");
        ok &= expect_false(restore_plan.apply_match_id, "same-generation capture keeps local match id");
        ok &= expect_false(restore_plan.apply_server_id, "same-generation capture keeps local server id");
        ok &= expect_false(restore_plan.apply_game_start_time, "same-generation capture keeps local start time");
        ok &= expect_true(
            restore_plan.connect_source == gbe::dota_lobby_state::SharedLobbyRestoreSource::LocalGenericCapture,
            "same-generation capture records local identity source");
        ok &= expect_true(
            restore_plan.state_source == gbe::dota_lobby_state::SharedLobbyRestoreSource::LocalGenericCapture,
            "same-generation capture records local launch source");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_source_aware_shared_runtime_restore_plan(lobby, restore_plan),
            "same-generation capture does not mutate Local lobby");
        ok &= expect_true(lobby.connect == "new-connect", "same-generation capture retains local connect value");
        ok &= expect_eq_u64(lobby.match_id, 22ull, "same-generation capture retains local match value");
    }

    // Same-generation runtime metadata keeps Local connect/server_id over an older shared snapshot.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 15ull;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_runtime_metadata(lobby, "local-connect", 77ull),
            "runtime metadata records local identity change");
        ok &= expect_eq_u64(
            lobby.runtime_metadata_generation,
            lobby.generation,
            "runtime metadata records source generation");

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation;
        shared.connect = "shared-connect";
        shared.server_id = 88ull;
        const auto restore_plan = gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
            lobby, shared, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_false(restore_plan.apply_connect, "same-generation runtime metadata keeps local connect");
        ok &= expect_false(restore_plan.apply_server_id, "same-generation runtime metadata keeps local server id");
        ok &= expect_true(
            restore_plan.connect_source == gbe::dota_lobby_state::SharedLobbyRestoreSource::LocalRuntimeMetadata,
            "same-generation runtime metadata records local connect source");
        ok &= expect_true(
            restore_plan.server_id_source == gbe::dota_lobby_state::SharedLobbyRestoreSource::LocalRuntimeMetadata,
            "same-generation runtime metadata records local server id source");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_source_aware_shared_runtime_restore_plan(lobby, restore_plan),
            "same-generation runtime metadata restore does not mutate Local lobby");
        ok &= expect_eq_str(lobby.connect, "local-connect", "same-generation runtime metadata retains local connect");
        ok &= expect_eq_u64(lobby.server_id, 77ull, "same-generation runtime metadata retains local server id");
    }

    // Same-generation generic capture keeps Local options/custom_game field groups over an older shared snapshot.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 10ull;
        gbe::dota_lobby_state::GenericLobbyCaptureInput input{};
        input.allow_cheats_raw = "1";
        input.fill_with_bots_raw = "1";
        input.visibility_raw = "2";
        input.bot_radiant_raw = "7";
        input.custom_game_mode = "local-mode";
        input.custom_map_name = "local-map";
        input.custom_game_id_raw = "88";
        const auto capture_plan = gbe::dota_lobby_state::compose_generic_lobby_capture_plan(
            lobby, input, GBE_kDotaLaunchPhaseSetupSynced);
        gbe::dota_lobby_state::apply_generic_lobby_capture_plan(lobby, capture_plan);

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation;
        shared.allow_cheats = false;
        shared.fill_with_bots = false;
        shared.visibility = 0u;
        shared.bot_radiant = 1ull;
        shared.custom_game.mode = "shared-mode";
        shared.custom_game.map_name = "shared-map";
        shared.custom_game.game_id = 11ull;
        const auto options_plan = gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(lobby, shared);
        ok &= expect_false(options_plan.apply_allow_cheats, "same-generation capture keeps local allow_cheats");
        ok &= expect_false(options_plan.apply_fill_with_bots, "same-generation capture keeps local fill_with_bots");
        ok &= expect_false(options_plan.apply_visibility, "same-generation capture keeps local visibility");
        ok &= expect_false(options_plan.apply_bot_radiant, "same-generation capture keeps local bot_radiant");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_shared_lobby_options_restore_plan(lobby, options_plan),
            "same-generation capture does not apply shared options");
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_custom_game_from_shared(lobby, shared),
            "same-generation capture does not apply shared custom game");
        ok &= expect_true(lobby.allow_cheats, "same-generation capture retains local allow_cheats");
        ok &= expect_true(lobby.fill_with_bots, "same-generation capture retains local fill_with_bots");
        ok &= expect_eq_u32(lobby.visibility, 2u, "same-generation capture retains local visibility");
        ok &= expect_eq_u64(lobby.bot_radiant, 7ull, "same-generation capture retains local bot_radiant");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 88ull, "same-generation capture retains local custom game id");
        ok &= expect_true(lobby.custom_game.map_name == "local-map", "same-generation capture retains local custom game map");
    }

    // steam auth ack: derive missing metadata and preserve values assigned earlier in the launch.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        const auto derived_plan = gbe::dota_lobby_state::compose_steam_auth_ack_launch_plan(lobby, 42u);
        gbe::dota_lobby_state::apply_steam_auth_ack_launch_plan(lobby, derived_plan);
        ok &= expect_eq_u32(lobby.launch_steam_auth_ticket_crc, 42u, "steam auth applies derived ticket CRC");
        ok &= expect_eq_u32(lobby.launch_steam_auth_message_sequence, 1u, "steam auth defaults sequence");
        ok &= expect_true(lobby.launch_steam_auth_ack_queued, "steam auth marks ack queued");

        lobby.launch_steam_auth_ticket_crc = 9u;
        lobby.launch_steam_auth_message_sequence = 7u;
        const auto existing_plan = gbe::dota_lobby_state::compose_steam_auth_ack_launch_plan(lobby, 42u);
        ok &= expect_eq_u32(existing_plan.ticket_crc, 9u, "steam auth keeps ticket CRC");
        ok &= expect_eq_u32(existing_plan.message_sequence, 7u, "steam auth keeps sequence");
    }

    // mark_launch_4511_seen: mark once for publish deduplication.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 9ull;
        ok &= expect_true(gbe::dota_lobby_state::mark_launch_4511_seen(lobby), "4511 marker changes on first notification");
        ok &= expect_true(lobby.launch_4511_seen, "4511 marker is retained");
        ok &= expect_false(gbe::dota_lobby_state::mark_launch_4511_seen(lobby), "4511 marker rejects duplicate notification");
        ok &= expect_eq_u64(lobby.launch_4511_generation, 9ull, "4511 marker records local source generation");
        ok &= expect_false(gbe::dota_lobby_state::restore_launch_4511_seen(lobby, true), "4511 restore keeps matching marker");
        ok &= expect_false(gbe::dota_lobby_state::restore_launch_4511_seen(lobby, false), "same-generation 4511 restore keeps local marker");
        ok &= expect_true(lobby.launch_4511_seen, "same-generation 4511 restore retains marker value");

        GBE_LocalLobby restore_lobby = make_active_lobby();
        restore_lobby.generation = 10ull;
        restore_lobby.launch_4511_seen = true;
        ok &= expect_true(gbe::dota_lobby_state::restore_launch_4511_seen(restore_lobby, false), "4511 restore applies shared marker without local source");
        ok &= expect_false(restore_lobby.launch_4511_seen, "4511 restore updates marker value without local source");
    }

    // Source-aware restore: shared identity fills Local values without a same-generation capture.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.connect = "1.2.3.4:27015";
        lobby.match_id = 100ull;
        lobby.game_start_time = 10u;
        lobby.room_name = "alpha";
        lobby.server_id = 30ull;
        GBE_SharedDotaLobbyState shared{};
        shared.connect = "5.6.7.8:27015";
        shared.match_id = 200ull;
        shared.server_id = 40ull;
        shared.game_start_time = 20u;
        shared.room_name = "beta";
        const auto plan = gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
            lobby, shared, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_true(plan.apply_connect, "source-aware restore applies shared endpoint");
        ok &= expect_true(plan.apply_match_id, "source-aware restore applies shared match id");
        ok &= expect_true(plan.apply_server_id, "source-aware restore applies shared server id");
        ok &= expect_true(plan.apply_game_start_time, "source-aware restore applies shared start time");
        ok &= expect_true(plan.apply_room_name, "source-aware restore applies shared room");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_source_aware_shared_runtime_restore_plan(lobby, plan),
            "source-aware restore reports identity change");
        ok &= expect_true(lobby.connect == "5.6.7.8:27015", "source-aware restore updates local endpoint");
        ok &= expect_eq_u64(lobby.match_id, 200ull, "source-aware restore updates local match id");
        ok &= expect_eq_u64(lobby.server_id, 40ull, "source-aware restore updates local server id");
        ok &= expect_eq_u32(lobby.game_start_time, 20u, "source-aware restore updates local start time");
        ok &= expect_true(lobby.room_name == "beta", "source-aware restore updates local room");
        lobby.owner_connected = true;
        lobby.owner_team = 2u;
        lobby.owner_slot = 3u;
        ok &= expect_false(gbe::dota_lobby_state::restore_lobby_owner_connected(lobby, true), "owner connected restore keeps matching value");
        ok &= expect_true(gbe::dota_lobby_state::restore_lobby_owner_connected(lobby, false), "owner connected restore applies shared value");
        ok &= expect_false(lobby.owner_connected, "owner connected restore updates local value");
        ok &= expect_false(gbe::dota_lobby_state::restore_lobby_owner_team(lobby, 2u), "owner team restore keeps matching value");
        ok &= expect_true(gbe::dota_lobby_state::restore_lobby_owner_team(lobby, 3u), "owner team restore applies shared value");
        ok &= expect_eq_u32(lobby.owner_team, 3u, "owner team restore updates local value");
        ok &= expect_false(gbe::dota_lobby_state::restore_lobby_owner_slot(lobby, 3u), "owner slot restore keeps matching value");
        ok &= expect_true(gbe::dota_lobby_state::restore_lobby_owner_slot(lobby, 4u), "owner slot restore applies shared value");
        ok &= expect_eq_u32(lobby.owner_slot, 4u, "owner slot restore updates local value");

        GBE_LocalLobby owner_lobby = make_active_lobby();
        owner_lobby.generation = 11ull;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_owner_connected(owner_lobby, true),
            "owner runtime local apply records owner connected source");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_owner_team(owner_lobby, 2u),
            "owner runtime local apply records owner team source");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_owner_slot(owner_lobby, 3u),
            "owner runtime local apply records owner slot source");
        GBE_SharedDotaLobbyState owner_shared{};
        owner_shared.generation = owner_lobby.generation;
        owner_shared.owner_connected = false;
        owner_shared.owner_team = 4u;
        owner_shared.owner_slot = 5u;
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_owner_runtime_from_shared(owner_lobby, owner_shared),
            "same-generation owner runtime keeps local owner fields");
        ok &= expect_true(owner_lobby.owner_connected, "same-generation owner runtime retains connected");
        ok &= expect_eq_u32(owner_lobby.owner_team, 2u, "same-generation owner runtime retains team");
        ok &= expect_eq_u32(owner_lobby.owner_slot, 3u, "same-generation owner runtime retains slot");

        GBE_LocalLobby no_source_owner_lobby = make_active_lobby();
        no_source_owner_lobby.owner_connected = true;
        no_source_owner_lobby.owner_team = 2u;
        no_source_owner_lobby.owner_slot = 3u;
        ok &= expect_true(
            gbe::dota_lobby_state::restore_lobby_owner_runtime_from_shared(no_source_owner_lobby, owner_shared),
            "owner runtime restore applies shared fields without local source");
        ok &= expect_false(no_source_owner_lobby.owner_connected, "owner runtime restore updates connected");
        ok &= expect_eq_u32(no_source_owner_lobby.owner_team, 4u, "owner runtime restore updates team");
        ok &= expect_eq_u32(no_source_owner_lobby.owner_slot, 5u, "owner runtime restore updates slot");

        GBE_DotaLobbyMemberState local_member{};
        local_member.steam_id = 1ull;
        local_member.account_id = 11u;
        local_member.team = 2u;
        local_member.slot = 3u;
        lobby.members = {local_member};
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_members(lobby, {local_member}),
            "members restore keeps matching members");
        GBE_DotaLobbyMemberState shared_member = local_member;
        shared_member.slot = 4u;
        ok &= expect_true(
            gbe::dota_lobby_state::restore_lobby_members(lobby, {shared_member}),
            "members restore applies changed members");
        ok &= expect_true(
            gbe::dota_lobby_flow::lobby_members_equal(lobby.members, {shared_member}),
            "members restore updates Local members");

        GBE_LocalLobby sourced_members_lobby = make_active_lobby();
        sourced_members_lobby.generation = 12ull;
        sourced_members_lobby.members = {local_member};
        sourced_members_lobby.members_generation = sourced_members_lobby.generation;
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_members(sourced_members_lobby, {shared_member}),
            "same-generation members restore keeps local members");
        ok &= expect_true(
            gbe::dota_lobby_flow::lobby_members_equal(sourced_members_lobby.members, {local_member}),
            "same-generation members restore retains local members");
    }

    // apply_lobby_owner_connected: local owner connection writes are idempotent.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.owner_connected = false;
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_owner_connected(lobby, false),
            "owner connected apply keeps matching disconnected value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_owner_connected(lobby, true),
            "owner connected apply reports connection change");
        ok &= expect_true(lobby.owner_connected, "owner connected apply updates local value");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_owner_connected(lobby, true),
            "owner connected apply keeps matching connected value");
    }

    // apply_lobby_owner_team / apply_lobby_owner_slot: local owner runtime writes are idempotent.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.owner_team = 2u;
        lobby.owner_slot = 3u;
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_owner_team(lobby, 2u),
            "owner team apply keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_owner_team(lobby, 4u),
            "owner team apply reports value change");
        ok &= expect_eq_u32(lobby.owner_team, 4u, "owner team apply updates local value");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_owner_slot(lobby, 3u),
            "owner slot apply keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_owner_slot(lobby, 5u),
            "owner slot apply reports value change");
        ok &= expect_eq_u32(lobby.owner_slot, 5u, "owner slot apply updates local value");
        lobby.owner_name = "old owner";
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_owner_name(lobby, "old owner"),
            "owner name apply keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_owner_name(lobby, "new owner"),
            "owner name apply reports value change");
        ok &= expect_eq_str(lobby.owner_name, "new owner", "owner name apply updates local value");
        ok &= expect_true(
            gbe::dota_lobby_state::note_generic_lobby_local_member_seen(lobby),
            "generic local seen applies initial observation flags");
        ok &= expect_true(lobby.seen_local_in_generic_lobby, "generic local seen marks observation");
        lobby.kicked_suppressed_logged = true;
        lobby.owner_adoption_suppressed_logged = true;
        ok &= expect_true(
            gbe::dota_lobby_state::note_generic_lobby_local_member_seen(lobby),
            "generic local seen clears suppression flags");
        ok &= expect_false(lobby.kicked_suppressed_logged, "generic local seen clears kicked suppression flag");
        ok &= expect_false(lobby.owner_adoption_suppressed_logged, "generic local seen clears owner adoption flag");
        ok &= expect_true(
            gbe::dota_lobby_state::mark_generic_lobby_waiting_join_confirmation_logged(lobby),
            "waiting join flag marks first log");
        ok &= expect_false(
            gbe::dota_lobby_state::mark_generic_lobby_waiting_join_confirmation_logged(lobby),
            "waiting join flag suppresses duplicate log");
        ok &= expect_true(
            gbe::dota_lobby_state::mark_generic_lobby_kicked_suppressed_logged(lobby),
            "kicked suppression flag marks first log");
        ok &= expect_false(
            gbe::dota_lobby_state::mark_generic_lobby_kicked_suppressed_logged(lobby),
            "kicked suppression flag suppresses duplicate log");
        ok &= expect_true(
            gbe::dota_lobby_state::mark_generic_lobby_owner_adoption_suppressed_logged(lobby),
            "owner adoption suppression flag marks first log");
        ok &= expect_false(
            gbe::dota_lobby_state::mark_generic_lobby_owner_adoption_suppressed_logged(lobby),
            "owner adoption suppression flag suppresses duplicate log");
        gbe::dota_lobby_state::clear_local_lobby(lobby);
        ok &= expect_false(lobby.active, "clear local lobby resets active flag");
        ok &= expect_eq_u64(lobby.lobby_id, 0ull, "clear local lobby resets lobby id");
        ok &= expect_true(lobby.members.empty(), "clear local lobby resets members");
        GBE_LocalLobby current = make_active_lobby();
        current.room_name = "before";
        GBE_LocalLobby snapshot = current;
        snapshot.room_name = "after";
        GBE_DotaLobbyMemberState kicked_snapshot_member{};
        kicked_snapshot_member.steam_id = 2ull;
        snapshot.members = {kicked_snapshot_member};
        gbe::dota_lobby_state::apply_lobby_member_kick_snapshot(current, snapshot);
        ok &= expect_eq_str(current.room_name, "after", "kick snapshot applies full Local snapshot");
        ok &= expect_true(
            gbe::dota_lobby_flow::lobby_members_equal(current.members, snapshot.members),
            "kick snapshot applies member list");
        ok &= expect_eq_u64(current.members_generation, current.generation, "kick snapshot records members source generation");
        GBE_LocalLobby create_target = make_active_lobby();
        create_target.lobby_id = 99ull;
        gbe::dota_lobby_state::CreateLobbyStateApplyPlan create_apply_plan{};
        create_apply_plan.lobby = snapshot;
        create_apply_plan.normalize_custom_game_details = true;
        gbe::dota_lobby_state::apply_create_lobby_state_plan(create_target, create_apply_plan);
        ok &= expect_eq_u64(create_target.lobby_id, snapshot.lobby_id, "create state apply replaces Local snapshot");
        ok &= expect_eq_str(create_target.room_name, "after", "create state apply applies plan lobby");
        ok &= expect_true(
            gbe::dota_lobby_flow::lobby_members_equal(create_target.members, snapshot.members),
            "create state apply preserves plan members");
        GBE_LocalLobby join_target = make_active_lobby();
        join_target.lobby_id = 123ull;
        gbe::dota_lobby_state::JoinLobbyMergePlan join_apply_plan{};
        join_apply_plan.lobby = snapshot;
        join_apply_plan.local_member.steam_id = 7ull;
        join_apply_plan.seen_local_in_generic_lobby = true;
        gbe::dota_lobby_state::apply_join_lobby_merge_plan(join_target, join_apply_plan);
        ok &= expect_eq_u64(join_target.lobby_id, snapshot.lobby_id, "join merge apply replaces Local snapshot");
        ok &= expect_eq_str(join_target.room_name, "after", "join merge apply applies plan lobby");
        ok &= expect_true(
            gbe::dota_lobby_flow::lobby_members_equal(join_target.members, snapshot.members),
            "join merge apply preserves plan members");
    }

    // apply_chat_channel / clear_chat_channel: local chat channel writes are grouped.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 21ull;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_chat_channel(lobby, 42ull, "lobby", 3u),
            "chat channel apply reports initial channel change");
        ok &= expect_true(lobby.has_chat_channel, "chat channel apply marks channel present");
        ok &= expect_eq_u64(lobby.chat_channel_id, 42ull, "chat channel apply updates channel id");
        ok &= expect_true(lobby.chat_channel_name == "lobby", "chat channel apply updates channel name");
        ok &= expect_eq_u32(lobby.chat_channel_type, 3u, "chat channel apply updates channel type");
        ok &= expect_eq_u64(
            lobby.chat_channel_generation,
            lobby.generation,
            "chat channel apply records source generation");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_chat_channel(lobby, 42ull, "lobby", 3u),
            "chat channel apply keeps matching values");
        ok &= expect_true(
            gbe::dota_lobby_state::clear_chat_channel(lobby),
            "chat channel clear reports existing channel change");
        ok &= expect_false(lobby.has_chat_channel, "chat channel clear marks channel absent");
        ok &= expect_eq_u64(lobby.chat_channel_id, 0ull, "chat channel clear resets channel id");
        ok &= expect_true(lobby.chat_channel_name.empty(), "chat channel clear resets channel name");
        ok &= expect_eq_u32(lobby.chat_channel_type, 0u, "chat channel clear resets channel type");
        ok &= expect_eq_u64(
            lobby.chat_channel_generation,
            lobby.generation,
            "chat channel clear records source generation");
        ok &= expect_false(
            gbe::dota_lobby_state::clear_chat_channel(lobby),
            "chat channel clear keeps empty channel state");
    }

    // apply_broadcast_channel / patch_broadcast_channel / clear_broadcast_channel: local broadcast writes are grouped.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 22ull;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_broadcast_channel(lobby, 7u, "US", "cast", "en"),
            "broadcast apply reports initial channel change");
        ok &= expect_true(lobby.has_broadcast_channel, "broadcast apply marks channel present");
        ok &= expect_eq_u32(lobby.broadcast_channel_id, 7u, "broadcast apply updates channel id");
        ok &= expect_true(lobby.broadcast_country_code == "US", "broadcast apply updates country");
        ok &= expect_true(lobby.broadcast_description == "cast", "broadcast apply updates description");
        ok &= expect_true(lobby.broadcast_language_code == "en", "broadcast apply updates language");
        ok &= expect_eq_u64(
            lobby.broadcast_channel_generation,
            lobby.generation,
            "broadcast apply records source generation");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_broadcast_channel(lobby, 7u, "US", "cast", "en"),
            "broadcast apply keeps matching values");
        ok &= expect_true(
            gbe::dota_lobby_state::patch_broadcast_channel(lobby, 7u, false, "", true, "cast2", false, ""),
            "broadcast patch reports optional field change");
        ok &= expect_true(lobby.broadcast_country_code == "US", "broadcast patch preserves absent country");
        ok &= expect_true(lobby.broadcast_description == "cast2", "broadcast patch updates present description");
        ok &= expect_true(lobby.broadcast_language_code == "en", "broadcast patch preserves absent language");
        ok &= expect_eq_u64(
            lobby.broadcast_channel_generation,
            lobby.generation,
            "broadcast patch records source generation");
        ok &= expect_false(
            gbe::dota_lobby_state::patch_broadcast_channel(lobby, 7u, false, "", false, "", false, ""),
            "broadcast patch keeps matching id and absent optional fields");
        ok &= expect_true(
            gbe::dota_lobby_state::clear_broadcast_channel(lobby, 8u),
            "broadcast clear reports existing channel change");
        ok &= expect_false(lobby.has_broadcast_channel, "broadcast clear marks channel absent");
        ok &= expect_eq_u32(lobby.broadcast_channel_id, 8u, "broadcast clear records request channel id");
        ok &= expect_true(lobby.broadcast_country_code.empty(), "broadcast clear resets country");
        ok &= expect_true(lobby.broadcast_description.empty(), "broadcast clear resets description");
        ok &= expect_true(lobby.broadcast_language_code.empty(), "broadcast clear resets language");
        ok &= expect_eq_u64(
            lobby.broadcast_channel_generation,
            lobby.generation,
            "broadcast clear records source generation");
    }

    // shared options restore: copy lobby options field group when shared differs.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.game_mode = 1u;
        lobby.server_region = 2u;
        lobby.lan = true;
        lobby.lan_host_ping_location = "cn";
        lobby.allow_cheats = false;
        lobby.fill_with_bots = true;
        lobby.allow_spectating = true;
        lobby.pass_key = "secret";
        lobby.visibility = 1u;
        lobby.bot_difficulty_radiant = 2u;
        lobby.bot_difficulty_dire = 3u;
        lobby.bot_radiant = 10ull;
        lobby.bot_dire = 20ull;
        GBE_SharedDotaLobbyState shared{};
        shared.game_mode = lobby.game_mode;
        shared.server_region = lobby.server_region;
        shared.lan = lobby.lan;
        shared.lan_host_ping_location = lobby.lan_host_ping_location;
        shared.allow_cheats = lobby.allow_cheats;
        shared.fill_with_bots = lobby.fill_with_bots;
        shared.allow_spectating = lobby.allow_spectating;
        shared.pass_key = lobby.pass_key;
        shared.visibility = lobby.visibility;
        shared.bot_difficulty_radiant = lobby.bot_difficulty_radiant;
        shared.bot_difficulty_dire = lobby.bot_difficulty_dire;
        shared.bot_radiant = lobby.bot_radiant;
        shared.bot_dire = lobby.bot_dire;
        const auto matching_plan = gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(lobby, shared);
        ok &= expect_false(
            gbe::dota_lobby_state::apply_shared_lobby_options_restore_plan(lobby, matching_plan),
            "options restore keeps matching field group");
        shared.game_mode = 5u;
        shared.server_region = 6u;
        shared.lan = false;
        shared.lan_host_ping_location = "us";
        shared.allow_cheats = true;
        shared.fill_with_bots = false;
        shared.allow_spectating = false;
        shared.pass_key = "open";
        shared.visibility = 0u;
        shared.bot_difficulty_radiant = 4u;
        shared.bot_difficulty_dire = 5u;
        shared.bot_radiant = 30ull;
        shared.bot_dire = 40ull;
        const auto changed_plan = gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(lobby, shared);
        ok &= expect_true(changed_plan.apply_game_mode, "options restore marks game_mode change");
        ok &= expect_true(changed_plan.apply_pass_key, "options restore marks pass_key change");
        ok &= expect_true(changed_plan.apply_bot_dire, "options restore marks bot_dire change");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_shared_lobby_options_restore_plan(lobby, changed_plan),
            "options restore reports field group change");
        ok &= expect_eq_u32(lobby.game_mode, 5u, "options restore updates game_mode");
        ok &= expect_eq_u32(lobby.server_region, 6u, "options restore updates server_region");
        ok &= expect_false(lobby.lan, "options restore updates lan");
        ok &= expect_true(lobby.lan_host_ping_location == "us", "options restore updates lan host ping");
        ok &= expect_true(lobby.allow_cheats, "options restore updates allow_cheats");
        ok &= expect_false(lobby.fill_with_bots, "options restore updates fill_with_bots");
        ok &= expect_false(lobby.allow_spectating, "options restore updates allow_spectating");
        ok &= expect_true(lobby.pass_key == "open", "options restore updates pass_key");
        ok &= expect_eq_u32(lobby.visibility, 0u, "options restore updates visibility");
        ok &= expect_eq_u32(lobby.bot_difficulty_radiant, 4u, "options restore updates radiant bot difficulty");
        ok &= expect_eq_u32(lobby.bot_difficulty_dire, 5u, "options restore updates dire bot difficulty");
        ok &= expect_eq_u64(lobby.bot_radiant, 30ull, "options restore updates radiant bots");
        ok &= expect_eq_u64(lobby.bot_dire, 40ull, "options restore updates dire bots");
    }

    // shared cache restore: copy cache field group when shared differs.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 5ull;
        lobby.has_cache_version = true;
        lobby.cache_version = 11ull;
        lobby.has_cache_service_id = true;
        lobby.cache_service_id = 22u;
        lobby.cache_service_list = {1u, 2u};
        lobby.has_cache_sync_version = true;
        lobby.cache_sync_version = 33ull;
        GBE_SharedDotaLobbyState shared{};
        shared.has_cache_version = lobby.has_cache_version;
        shared.cache_version = lobby.cache_version;
        shared.has_cache_service_id = lobby.has_cache_service_id;
        shared.cache_service_id = lobby.cache_service_id;
        shared.cache_service_list = lobby.cache_service_list;
        shared.has_cache_sync_version = lobby.has_cache_sync_version;
        shared.cache_sync_version = lobby.cache_sync_version;
        const auto matching_plan = gbe::dota_lobby_state::compose_shared_lobby_cache_restore_plan(lobby, shared);
        ok &= expect_false(
            gbe::dota_lobby_state::apply_shared_lobby_cache_restore_plan(lobby, matching_plan),
            "cache restore keeps matching field group");
        shared.has_cache_version = false;
        shared.cache_version = 0ull;
        shared.has_cache_service_id = false;
        shared.cache_service_id = 0u;
        shared.cache_service_list = {9u};
        shared.has_cache_sync_version = false;
        shared.cache_sync_version = 0ull;
        const auto changed_plan = gbe::dota_lobby_state::compose_shared_lobby_cache_restore_plan(lobby, shared);
        ok &= expect_true(changed_plan.apply_cache_version, "cache restore marks version change");
        ok &= expect_true(changed_plan.apply_cache_service_id, "cache restore marks service id change");
        ok &= expect_true(changed_plan.apply_cache_service_list, "cache restore marks service list change");
        ok &= expect_true(changed_plan.apply_cache_sync_version, "cache restore marks sync version change");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_shared_lobby_cache_restore_plan(lobby, changed_plan),
            "cache restore reports field group change");
        ok &= expect_false(lobby.has_cache_version, "cache restore updates has_cache_version");
        ok &= expect_eq_u64(lobby.cache_version, 0ull, "cache restore updates cache_version");
        ok &= expect_false(lobby.has_cache_service_id, "cache restore updates has_cache_service_id");
        ok &= expect_eq_u32(lobby.cache_service_id, 0u, "cache restore updates cache_service_id");
        ok &= expect_true(lobby.cache_service_list.size() == 1u && lobby.cache_service_list[0] == 9u,
            "cache restore updates cache_service_list");
        ok &= expect_false(lobby.has_cache_sync_version, "cache restore updates has_cache_sync_version");
        ok &= expect_eq_u64(lobby.cache_sync_version, 0ull, "cache restore updates cache_sync_version");
    }

    // Same-generation cache metadata capture keeps the Local cache field group over shared restore.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 6ull;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_cache_subscription_metadata(
                lobby, true, 44ull, true, 55u, {3u, 4u}, true, 66ull),
            "cache metadata capture records local cache field group");
        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation;
        shared.has_cache_version = false;
        shared.cache_version = 0ull;
        shared.has_cache_service_id = false;
        shared.cache_service_id = 0u;
        shared.cache_service_list = {9u};
        shared.has_cache_sync_version = false;
        shared.cache_sync_version = 0ull;
        const auto plan = gbe::dota_lobby_state::compose_shared_lobby_cache_restore_plan(lobby, shared);
        ok &= expect_false(plan.apply_cache_version, "same-generation cache metadata keeps local cache version");
        ok &= expect_false(plan.apply_cache_service_id, "same-generation cache metadata keeps local service id");
        ok &= expect_false(plan.apply_cache_service_list, "same-generation cache metadata keeps local service list");
        ok &= expect_false(plan.apply_cache_sync_version, "same-generation cache metadata keeps local sync version");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_shared_lobby_cache_restore_plan(lobby, plan),
            "same-generation cache metadata does not apply shared cache");
        ok &= expect_true(lobby.has_cache_version, "same-generation cache metadata retains cache version flag");
        ok &= expect_eq_u64(lobby.cache_version, 44ull, "same-generation cache metadata retains cache version");
        ok &= expect_true(lobby.has_cache_service_id, "same-generation cache metadata retains service id flag");
        ok &= expect_eq_u32(lobby.cache_service_id, 55u, "same-generation cache metadata retains service id");
        ok &= expect_true(lobby.cache_service_list.size() == 2u && lobby.cache_service_list[1] == 4u,
            "same-generation cache metadata retains service list");
        ok &= expect_true(lobby.has_cache_sync_version, "same-generation cache metadata retains sync version flag");
        ok &= expect_eq_u64(lobby.cache_sync_version, 66ull, "same-generation cache metadata retains sync version");
    }

    // apply_cache_subscription_metadata: local cache metadata writes are grouped.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        ok &= expect_true(
            gbe::dota_lobby_state::apply_cache_subscription_metadata(
                lobby, true, 100ull, true, 10u, {1u, 2u}, true, 200ull),
            "cache metadata apply reports initial field group change");
        ok &= expect_true(lobby.has_cache_version, "cache metadata apply marks version present");
        ok &= expect_eq_u64(lobby.cache_version, 100ull, "cache metadata apply updates version");
        ok &= expect_true(lobby.has_cache_service_id, "cache metadata apply marks service id present");
        ok &= expect_eq_u32(lobby.cache_service_id, 10u, "cache metadata apply updates service id");
        ok &= expect_true(lobby.cache_service_list.size() == 2u && lobby.cache_service_list[1] == 2u,
            "cache metadata apply updates service list");
        ok &= expect_true(lobby.has_cache_sync_version, "cache metadata apply marks sync version present");
        ok &= expect_eq_u64(lobby.cache_sync_version, 200ull, "cache metadata apply updates sync version");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_cache_subscription_metadata(
                lobby, true, 100ull, true, 10u, {1u, 2u}, true, 200ull),
            "cache metadata apply keeps matching field group");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_cache_subscription_metadata(
                lobby, false, 0ull, false, 0u, {}, false, 0ull),
            "cache metadata apply reports cleared field group change");
        ok &= expect_false(lobby.has_cache_version, "cache metadata apply clears version present flag");
        ok &= expect_eq_u64(lobby.cache_version, 0ull, "cache metadata apply clears version");
        ok &= expect_false(lobby.has_cache_service_id, "cache metadata apply clears service id present flag");
        ok &= expect_eq_u32(lobby.cache_service_id, 0u, "cache metadata apply clears service id");
        ok &= expect_true(lobby.cache_service_list.empty(), "cache metadata apply clears service list");
        ok &= expect_false(lobby.has_cache_sync_version, "cache metadata apply clears sync version present flag");
        ok &= expect_eq_u64(lobby.cache_sync_version, 0ull, "cache metadata apply clears sync version");
    }

    // restore_lobby_custom_game: shared custom game details restore.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 11ull;
        lobby.custom_game.map_name = "map_a";
        GBE_DotaCustomGameDetails matching = lobby.custom_game;
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_custom_game(lobby, matching),
            "custom game restore keeps matching details");
        GBE_DotaCustomGameDetails shared{};
        shared.game_id = 22ull;
        shared.map_name = "map_b";
        shared.max_players = 10u;
        ok &= expect_true(
            gbe::dota_lobby_state::restore_lobby_custom_game(lobby, shared),
            "custom game restore applies shared details");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 22ull, "custom game restore updates game_id");
        ok &= expect_true(lobby.custom_game.map_name == "map_b", "custom game restore updates map_name");
        ok &= expect_eq_u32(lobby.custom_game.max_players, 10u, "custom game restore updates max_players");
    }

    // apply_lobby_bot_difficulty_for_team: team selects radiant or dire field.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 26ull;
        lobby.bot_difficulty_radiant = 1u;
        lobby.bot_difficulty_dire = 2u;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_bot_difficulty_for_team(lobby, GBE_kDotaTeamGoodGuys, 3u),
            "bot difficulty apply updates radiant team");
        ok &= expect_eq_u32(lobby.bot_difficulty_radiant, 3u, "bot difficulty apply stores radiant difficulty");
        ok &= expect_eq_u32(lobby.bot_difficulty_dire, 2u, "bot difficulty apply preserves dire difficulty");
        ok &= expect_eq_u64(
            lobby.bot_difficulty_generation,
            lobby.generation,
            "bot difficulty apply records source generation");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_bot_difficulty_for_team(lobby, GBE_kDotaTeamBadGuys, 4u),
            "bot difficulty apply updates dire team");
        ok &= expect_eq_u32(lobby.bot_difficulty_radiant, 3u, "bot difficulty apply preserves radiant difficulty");
        ok &= expect_eq_u32(lobby.bot_difficulty_dire, 4u, "bot difficulty apply stores dire difficulty");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_bot_difficulty_for_team(lobby, GBE_kDotaTeamBadGuys, 4u),
            "bot difficulty apply keeps matching dire value");
    }

    // Same-generation bot difficulty apply keeps Local options over older shared options.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 27ull;
        lobby.bot_difficulty_radiant = 1u;
        lobby.bot_difficulty_dire = 2u;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_bot_difficulty_for_team(lobby, GBE_kDotaTeamGoodGuys, 3u),
            "bot difficulty source apply updates local options");

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation;
        shared.server_region = 9u;
        shared.bot_difficulty_radiant = 5u;
        shared.bot_difficulty_dire = 6u;
        const auto options_plan = gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(lobby, shared);
        ok &= expect_true(options_plan.apply_server_region, "same-generation bot difficulty allows unrelated options");
        ok &= expect_false(options_plan.apply_bot_difficulty_radiant, "same-generation bot difficulty keeps radiant");
        ok &= expect_false(options_plan.apply_bot_difficulty_dire, "same-generation bot difficulty keeps dire");
    }

    // Older bot difficulty source marker accepts shared options.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 27ull;
        lobby.bot_difficulty_radiant = 1u;
        lobby.bot_difficulty_dire = 2u;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_bot_difficulty_for_team(lobby, GBE_kDotaTeamGoodGuys, 3u),
            "bot difficulty source apply updates local radiant");

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation + 1ull;
        shared.bot_difficulty_radiant = 5u;
        shared.bot_difficulty_dire = 6u;
        const auto options_plan = gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(lobby, shared);
        ok &= expect_true(options_plan.apply_bot_difficulty_radiant, "newer shared restore applies radiant bot difficulty");
        ok &= expect_true(options_plan.apply_bot_difficulty_dire, "newer shared restore applies dire bot difficulty");
    }

    // apply_custom_game_loading_metadata: 8052 runtime metadata preserves absent values.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 14ull;
        lobby.custom_game.game_id = 10ull;
        lobby.game_start_time = 20u;
        ok &= expect_false(
            gbe::dota_lobby_state::apply_custom_game_loading_metadata(lobby, 0ull, 0u),
            "custom game loading metadata keeps absent values");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 10ull, "custom game loading metadata preserves game id");
        ok &= expect_eq_u32(lobby.game_start_time, 20u, "custom game loading metadata preserves start time");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_custom_game_loading_metadata(lobby, 30ull, 40u),
            "custom game loading metadata reports changed values");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 30ull, "custom game loading metadata updates game id");
        ok &= expect_eq_u32(lobby.game_start_time, 40u, "custom game loading metadata updates start time");
        ok &= expect_eq_u64(lobby.custom_game_loading_generation, 14ull, "custom game loading metadata records local source generation");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_custom_game_loading_metadata(lobby, 30ull, 40u),
            "custom game loading metadata keeps matching values");

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation;
        shared.custom_game.game_id = 50ull;
        shared.game_start_time = 60u;
        const auto runtime_plan = gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
            lobby,
            shared,
            GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_false(runtime_plan.apply_game_start_time, "same-generation custom game loading keeps local start time");
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_custom_game_from_shared(lobby, shared),
            "same-generation custom game loading keeps local custom game");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 30ull, "same-generation custom game loading retains game id");
        ok &= expect_eq_u32(lobby.game_start_time, 40u, "same-generation custom game loading retains start time");
    }

    // restore_lobby_generation / restore_lobby_generic_lobby_id: identity restore.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 7ull;
        lobby.generic_lobby_id = 70ull;
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_generation(lobby, 7ull),
            "generation restore keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::restore_lobby_generation(lobby, 8ull),
            "generation restore applies shared value");
        ok &= expect_eq_u64(lobby.generation, 8ull, "generation restore updates local value");
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_generic_lobby_id(lobby, 70ull),
            "generic lobby id restore keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::restore_lobby_generic_lobby_id(lobby, 80ull),
            "generic lobby id restore applies shared value");
        ok &= expect_eq_u64(lobby.generic_lobby_id, 80ull, "generic lobby id restore updates local value");
    }

    // apply_lobby_generation: generation applies and reports identity changes.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 10ull;
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_generation(lobby, 10ull),
            "generation apply keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_generation(lobby, 20ull),
            "generation apply reports changed value");
        ok &= expect_eq_u64(lobby.generation, 20ull, "generation apply updates local value");
    }

    // apply_lobby_generic_lobby_id: generic lobby id applies and clears identity.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generic_lobby_id = 10ull;
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_generic_lobby_id(lobby, 10ull),
            "generic lobby id apply keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_generic_lobby_id(lobby, 20ull),
            "generic lobby id apply reports changed value");
        ok &= expect_eq_u64(lobby.generic_lobby_id, 20ull, "generic lobby id apply updates local value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_generic_lobby_id(lobby, 0ull),
            "generic lobby id apply clears local value");
        ok &= expect_eq_u64(lobby.generic_lobby_id, 0ull, "generic lobby id apply stores zero clear value");
    }

    // apply_source_tv_metadata: SourceTV field group preserves absent values.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.tv_secret_code = 11ull;
        lobby.tv_port = 22u;
        ok &= expect_false(
            gbe::dota_lobby_state::apply_source_tv_metadata(lobby, 0ull, 0u),
            "SourceTV metadata apply preserves zero inputs");
        ok &= expect_eq_u64(lobby.tv_secret_code, 11ull, "SourceTV metadata keeps secret on zero input");
        ok &= expect_eq_u32(lobby.tv_port, 22u, "SourceTV metadata keeps port on zero input");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_source_tv_metadata(lobby, 33ull, 44u),
            "SourceTV metadata apply reports field changes");
        ok &= expect_eq_u64(lobby.tv_secret_code, 33ull, "SourceTV metadata updates secret");
        ok &= expect_eq_u32(lobby.tv_port, 44u, "SourceTV metadata updates port");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_source_tv_metadata(lobby, 33ull, 44u),
            "SourceTV metadata apply reports matching values as no-op");
    }

    // apply_runtime_metadata: metadata publish applies connect and server id exactly.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.connect = "1.2.3.4:27015";
        lobby.server_id = 10ull;
        ok &= expect_true(
            gbe::dota_lobby_state::apply_runtime_metadata(lobby, "", 20ull),
            "runtime metadata applies empty connect values");
        ok &= expect_eq_str(lobby.connect, "", "runtime metadata clears connect");
        ok &= expect_eq_u64(lobby.server_id, 20ull, "runtime metadata updates server id with empty connect");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_runtime_metadata(lobby, "5.6.7.8:27015", 20ull),
            "runtime metadata reports connect change");
        ok &= expect_eq_str(lobby.connect, "5.6.7.8:27015", "runtime metadata updates connect");
        ok &= expect_eq_u64(lobby.server_id, 20ull, "runtime metadata preserves matching server id");
        ok &= expect_eq_u64(
            lobby.runtime_metadata_generation,
            lobby.generation,
            "runtime metadata stores source generation after identity change");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_runtime_metadata(lobby, "5.6.7.8:27015", 20ull),
            "runtime metadata keeps matching values");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_runtime_metadata(lobby, "5.6.7.8:27015", 0ull),
            "runtime metadata applies zero server id input");
        ok &= expect_eq_u64(lobby.server_id, 0ull, "runtime metadata clears server id on zero input");
    }

    // apply_lobby_details_update: 7046 details update applies options and custom game fields.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 24ull;
        lobby.room_name = "old room";
        lobby.server_region = 1u;
        lobby.lan = false;
        lobby.allow_cheats = false;
        lobby.pass_key = "old";
        lobby.custom_game.game_id = 10ull;

        gbe::proto_wire::DotaPracticeLobbyDetailsRequest details{};
        details.has_room_name = true;
        details.room_name = "new room";
        details.has_server_region = true;
        details.server_region = 3u;
        details.has_lan = true;
        details.lan = true;
        details.has_allow_cheats = true;
        details.allow_cheats = true;
        details.has_pass_key = true;
        details.pass_key = "new";
        details.has_custom_game_id = true;
        details.custom_game_id = 20ull;
        gbe::dota_lobby_state::apply_lobby_details_update(lobby, details);

        ok &= expect_eq_str(lobby.room_name, "new room", "details update applies room name");
        ok &= expect_eq_u32(lobby.server_region, 3u, "details update applies server region");
        ok &= expect_true(lobby.lan, "details update applies lan flag");
        ok &= expect_true(lobby.allow_cheats, "details update applies cheats flag");
        ok &= expect_eq_str(lobby.pass_key, "new", "details update applies pass key");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 20ull, "details update applies custom game id");
        ok &= expect_eq_u64(
            lobby.details_runtime_generation,
            lobby.generation,
            "details update records runtime source generation");
        ok &= expect_eq_u64(
            lobby.details_options_generation,
            lobby.generation,
            "details update records options source generation");
        ok &= expect_eq_u64(
            lobby.details_custom_game_generation,
            lobby.generation,
            "details update records custom game source generation");
    }

    // Same-generation details update keeps Local room/options/custom_game over older shared fields.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 25ull;
        gbe::proto_wire::DotaPracticeLobbyDetailsRequest details{};
        details.has_room_name = true;
        details.room_name = "local room";
        details.has_server_region = true;
        details.server_region = 5u;
        details.has_allow_cheats = true;
        details.allow_cheats = true;
        details.has_custom_game_id = true;
        details.custom_game_id = 90ull;
        gbe::dota_lobby_state::apply_lobby_details_update(lobby, details);

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation;
        shared.room_name = "shared room";
        shared.server_region = 2u;
        shared.allow_cheats = false;
        shared.custom_game.game_id = 10ull;
        const auto runtime_plan = gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
            lobby, shared, GBE_kDotaLaunchPhaseRunQueued);
        const auto options_plan = gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(lobby, shared);

        ok &= expect_false(runtime_plan.apply_room_name, "same-generation details update keeps local room");
        ok &= expect_true(
            runtime_plan.room_name_source == gbe::dota_lobby_state::SharedLobbyRestoreSource::LocalDetailsUpdate,
            "same-generation details update records room source");
        ok &= expect_false(options_plan.apply_server_region, "same-generation details update keeps local server region");
        ok &= expect_false(options_plan.apply_allow_cheats, "same-generation details update keeps local cheats flag");
        ok &= expect_false(
            gbe::dota_lobby_state::restore_lobby_custom_game_from_shared(lobby, shared),
            "same-generation details update keeps local custom game");
        ok &= expect_eq_str(lobby.room_name, "local room", "details restore retains local room");
        ok &= expect_eq_u32(lobby.server_region, 5u, "details restore retains local server region");
        ok &= expect_true(lobby.allow_cheats, "details restore retains local cheats flag");
        ok &= expect_eq_u64(lobby.custom_game.game_id, 90ull, "details restore retains local custom game id");
    }

    // Older details source markers accept shared room/options/custom_game fields.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 25ull;
        gbe::proto_wire::DotaPracticeLobbyDetailsRequest details{};
        details.has_room_name = true;
        details.room_name = "local room";
        details.has_server_region = true;
        details.server_region = 5u;
        details.has_allow_cheats = true;
        details.allow_cheats = true;
        details.has_custom_game_id = true;
        details.custom_game_id = 90ull;
        gbe::dota_lobby_state::apply_lobby_details_update(lobby, details);

        GBE_SharedDotaLobbyState shared{};
        shared.generation = lobby.generation + 1ull;
        shared.room_name = "shared room";
        shared.server_region = 2u;
        shared.allow_cheats = false;
        shared.custom_game.game_id = 10ull;
        const auto runtime_plan = gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
            lobby, shared, GBE_kDotaLaunchPhaseRunQueued);
        const auto options_plan = gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(lobby, shared);
        ok &= expect_true(runtime_plan.apply_room_name, "newer shared restore applies room after details marker ages out");
        ok &= expect_true(options_plan.apply_server_region, "newer shared restore applies server region after details marker ages out");
        ok &= expect_true(options_plan.apply_allow_cheats, "newer shared restore applies cheats after details marker ages out");
        ok &= expect_true(
            gbe::dota_lobby_state::restore_lobby_custom_game_from_shared(lobby, shared),
            "newer shared restore applies custom game after details marker ages out");
    }

    // apply_lobby_server_id: server id applies exact runtime identity.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.server_id = 10ull;
        ok &= expect_false(
            gbe::dota_lobby_state::apply_lobby_server_id(lobby, 10ull),
            "server id apply keeps matching value");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_server_id(lobby, 20ull),
            "server id apply reports changed value");
        ok &= expect_eq_u64(lobby.server_id, 20ull, "server id apply updates local value");
        ok &= expect_eq_u64(
            lobby.runtime_metadata_generation,
            lobby.generation,
            "server id apply records runtime metadata generation");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_lobby_server_id(lobby, 0ull),
            "server id apply clears local value");
        ok &= expect_eq_u64(lobby.server_id, 0ull, "server id apply stores zero clear value");
    }

    // apply_runtime_connect: 4508 runtime endpoint preserves absent values.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.connect = "1.2.3.4:27015";
        ok &= expect_false(
            gbe::dota_lobby_state::apply_runtime_connect(lobby, ""),
            "runtime connect apply preserves empty input");
        ok &= expect_true(lobby.connect == "1.2.3.4:27015", "runtime connect keeps existing endpoint");
        ok &= expect_true(
            gbe::dota_lobby_state::apply_runtime_connect(lobby, "5.6.7.8:27015"),
            "runtime connect apply reports endpoint change");
        ok &= expect_true(lobby.connect == "5.6.7.8:27015", "runtime connect updates endpoint");
        ok &= expect_eq_u64(
            lobby.runtime_metadata_generation,
            lobby.generation,
            "runtime connect apply records runtime metadata generation");
        ok &= expect_false(
            gbe::dota_lobby_state::apply_runtime_connect(lobby, "5.6.7.8:27015"),
            "runtime connect apply reports matching endpoint as no-op");
    }

    // apply_lifecycle_lobby_state: lifecycle action writes state fields together.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        gbe::dota_lobby_state::apply_lifecycle_lobby_state(lobby, 5u, 7u);
        ok &= expect_eq_u32(lobby.state, 5u, "lifecycle state apply updates state");
        ok &= expect_eq_u32(lobby.game_state, 7u, "lifecycle state apply updates game state");
    }

    // apply_postgame_lobby_state_plan: postgame action patches state, chat, and cache together.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 5ull;
        lobby.has_cache_version = true;
        lobby.cache_version = 9u;
        lobby.has_cache_service_id = true;
        lobby.cache_service_id = 11u;
        lobby.cache_service_list = {1u, 2u};
        lobby.has_cache_sync_version = true;
        lobby.cache_sync_version = 13u;
        lobby.abandon_postgame_active = false;

        gbe::dota_lobby_state::PostGameLobbyStateApplyPlan plan{};
        plan.state = 3u;
        plan.game_state = 6u;
        plan.chat_channel_id = 42ull;
        plan.chat_channel_name = "postgame";
        plan.abandon_pre_postgame_chat_channel_id = 7ull;
        gbe::dota_lobby_state::apply_postgame_lobby_state_plan(lobby, plan);

        ok &= expect_eq_u32(lobby.state, 3u, "postgame plan updates state");
        ok &= expect_eq_u32(lobby.game_state, 6u, "postgame plan updates game state");
        ok &= expect_true(lobby.has_chat_channel, "postgame plan enables chat channel");
        ok &= expect_eq_u64(lobby.chat_channel_id, 42ull, "postgame plan sets chat channel id");
        ok &= expect_true(lobby.chat_channel_name == "postgame", "postgame plan sets chat channel name");
        ok &= expect_eq_u32(lobby.chat_channel_type, 18u, "postgame plan sets chat channel type");
        ok &= expect_eq_u64(lobby.abandon_pre_postgame_chat_channel_id, 7ull, "postgame plan keeps pre channel");
        ok &= expect_true(lobby.postgame_chat_tombstone_active, "postgame plan enables chat tombstone");
        ok &= expect_eq_u64(lobby.postgame_chat_tombstone_channel_id, 7ull, "postgame tombstone records pre channel");
        ok &= expect_eq_u64(lobby.postgame_chat_tombstone_generation, 5ull, "postgame tombstone records generation");
        ok &= expect_true(
            gbe::dota_lobby_state::postgame_chat_tombstone_matches(lobby, 7ull),
            "postgame tombstone matches pre channel in same generation");
        ok &= expect_false(
            gbe::dota_lobby_state::postgame_chat_tombstone_matches(lobby, 8ull),
            "postgame tombstone rejects other channels");
        lobby.generation = 6ull;
        ok &= expect_false(
            gbe::dota_lobby_state::postgame_chat_tombstone_matches(lobby, 7ull),
            "postgame tombstone rejects stale generation");
        lobby.generation = 5ull;
        gbe::dota_lobby_state::clear_postgame_chat_tombstone(lobby);
        ok &= expect_false(lobby.postgame_chat_tombstone_active, "postgame tombstone clear disables tombstone");
        ok &= expect_eq_u64(lobby.postgame_chat_tombstone_channel_id, 0ull, "postgame tombstone clear resets channel");
        ok &= expect_eq_u64(lobby.postgame_chat_tombstone_generation, 0ull, "postgame tombstone clear resets generation");
        ok &= expect_eq_u64(lobby.abandon_pre_postgame_chat_channel_id, 0ull, "postgame tombstone clear resets legacy pre channel");
        ok &= expect_false(lobby.has_cache_version, "postgame plan clears cache version flag");
        ok &= expect_eq_u32(lobby.cache_version, 0u, "postgame plan clears cache version");
        ok &= expect_false(lobby.has_cache_service_id, "postgame plan clears cache service flag");
        ok &= expect_eq_u64(lobby.cache_service_id, 0ull, "postgame plan clears cache service id");
        ok &= expect_true(lobby.cache_service_list.empty(), "postgame plan clears cache service list");
        ok &= expect_false(lobby.has_cache_sync_version, "postgame plan clears cache sync flag");
        ok &= expect_eq_u32(lobby.cache_sync_version, 0u, "postgame plan clears cache sync version");
        ok &= expect_true(lobby.abandon_postgame_active, "postgame plan marks abandon postgame active");
    }

    // compose_queued_lobby_state_apply_plan: state=1 + game_state=0 + sync -> bump to setup_synced.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.match_id = 1ull;
        lobby.game_start_time = 1u;
        lobby.connect = "1.2.3.4:27015";
        lobby.launch_phase = GBE_kDotaLaunchPhaseRequested;  // below setup_synced
        auto plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
            lobby, 1u, 0u, false, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_eq_u32(plan.state, 1u, "queued apply state");
        ok &= expect_eq_u32(plan.launch_phase, GBE_kDotaLaunchPhaseSetupSynced, "queued apply setup_synced bump");
        ok &= expect_false(plan.preserved_game_state, "queued apply no preserve when game_state zero");
    }

    // compose_queued_lobby_state_apply_plan: state=2 + game_state=0 -> bump to run_queued.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.launch_phase = GBE_kDotaLaunchPhaseRequested;  // below run_queued
        auto plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
            lobby, 2u, 0u, false, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_eq_u32(plan.state, 2u, "queued apply run state");
        ok &= expect_eq_u32(plan.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "queued apply run_queued bump");
    }

    // compose_queued_lobby_state_apply_plan: state=2 + game_state>0 -> no phase bump from zero-state branch.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;  // already above run_queued? no, setup_synced < run_queued
        auto plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
            lobby, 2u, 5u, false, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_eq_u32(plan.game_state, 5u, "queued apply keeps nonzero game_state");
        // launch_phase stays at setup_synced because the state=2/game_state=0 branch does not fire
        ok &= expect_eq_u32(plan.launch_phase, GBE_kDotaLaunchPhaseSetupSynced, "queued apply no bump for nonzero game_state");
    }

    // compose_practice_lobby_launch_event_plan: 7041 emits the initial details update and setup presence intent.
    {
        auto plan = gbe::dota_lobby_state::compose_practice_lobby_launch_event_plan(26u);
        ok &= expect_true(plan.initial_details.send, "7041 initial details is sent");
        ok &= expect_eq_u32(plan.initial_details.emsg, 26u, "7041 initial details emsg");
        ok &= expect_eq_str(plan.initial_details.reason, "7041_initial_26", "7041 initial details reason");
        ok &= expect_true(plan.presence.update, "7041 presence update intent");
        ok &= expect_eq_str(plan.presence.persona_reason, "7041_launch_init", "7041 persona reason");
    }

    return ok;
}

bool test_launch_lifecycle_transition_decision()
{
    bool ok = true;

    // 7070 ready-up: custom game RUN state advances to wait-for-players and requests publish + details update.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 0u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        auto d = gbe::dota_lobby_state::compute_custom_game_ready_up_transition(
            lobby,
            1u,
            GBE_kDotaLaunchPhaseRunQueued,
            "7070_custom_game_ready_up_run_ack");
        ok &= expect_true(d.apply_lobby_state, "7070 applies lobby state");
        ok &= expect_eq_u32(d.next_state, 2u, "7070 next state");
        ok &= expect_eq_u32(d.next_game_state, 1u, "7070 next game_state");
        ok &= expect_true(d.publish_shared_state, "7070 publishes shared state");
        ok &= expect_true(d.send_details_update, "7070 sends details update");
        ok &= expect_eq_str(d.reason, "7070_custom_game_ready_up_run_ack", "7070 reason");
    }

    // 7070 ready_state=0 only sends the ready-up status response at the handler layer.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 0u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        auto d = gbe::dota_lobby_state::compute_custom_game_ready_up_transition(
            lobby,
            0u,
            GBE_kDotaLaunchPhaseRunQueued,
            "7070_custom_game_ready_up_run_ack");
        ok &= expect_false(d.apply_lobby_state, "7070 ready_state zero skips transition");
        ok &= expect_false(d.send_details_update, "7070 ready_state zero skips details update");
    }

    // 8052 after server setup sync advances to RUN through the runtime update queue.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 1u;
        lobby.game_state = 0u;
        lobby.match_id = 600ull;
        lobby.game_start_time = 111u;
        lobby.connect = "1.2.3.4:27015";
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
        auto d = gbe::dota_lobby_state::compute_custom_game_started_loading_transition(
            lobby,
            true,
            GBE_kDotaLaunchPhaseSetupSynced,
            GBE_kDotaLaunchPhaseRunQueued,
            "8052_started_loading");
        ok &= expect_true(d.apply_lobby_state, "8052 applies run state");
        ok &= expect_true(d.mark_launch_phase, "8052 marks run phase");
        ok &= expect_eq_u32(d.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "8052 launch phase");
        ok &= expect_eq_u32(d.next_state, 2u, "8052 next state");
        ok &= expect_eq_u32(d.next_game_state, 0u, "8052 next game_state");
        ok &= expect_true(d.queue_runtime_lobby_update, "8052 queues runtime update");
        ok &= expect_false(d.publish_shared_state, "8052 run advance skips fallback publish intent");
    }

    // 8052 before setup sync falls back to publish + details update.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 0u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        auto d = gbe::dota_lobby_state::compute_custom_game_started_loading_transition(
            lobby,
            true,
            GBE_kDotaLaunchPhaseSetupSynced,
            GBE_kDotaLaunchPhaseRunQueued,
            "8052_started_loading");
        ok &= expect_false(d.queue_runtime_lobby_update, "8052 fallback skips runtime queue");
        ok &= expect_true(d.publish_shared_state, "8052 fallback publishes shared state");
        ok &= expect_true(d.send_details_update, "8052 fallback sends details update");
    }

    // 8053 success marks Loaded and keeps game_state at least wait-for-players.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 1u;
        lobby.game_state = 0u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        auto d = gbe::dota_lobby_state::compute_custom_game_finished_loading_transition(
            lobby,
            true,
            false,
            GBE_kDotaLaunchPhaseRunQueued,
            GBE_kDotaLaunchPhaseLoaded,
            "8053_finished_loading");
        ok &= expect_true(d.apply_lobby_state, "8053 applies lobby state");
        ok &= expect_eq_u32(d.next_state, 2u, "8053 next state");
        ok &= expect_eq_u32(d.next_game_state, 1u, "8053 next game_state floor");
        ok &= expect_true(d.mark_launch_phase, "8053 success marks loaded");
        ok &= expect_eq_u32(d.launch_phase, GBE_kDotaLaunchPhaseLoaded, "8053 loaded phase");
        ok &= expect_true(d.publish_shared_state, "8053 publishes shared state");
        ok &= expect_true(d.send_details_update, "8053 sends details update");
    }

    // 8053 load failure publishes state/details without marking the launch loaded.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 0u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        auto d = gbe::dota_lobby_state::compute_custom_game_finished_loading_transition(
            lobby,
            true,
            true,
            GBE_kDotaLaunchPhaseRunQueued,
            GBE_kDotaLaunchPhaseLoaded,
            "8053_load_failed");
        ok &= expect_false(d.mark_launch_phase, "8053 failure leaves phase unchanged");
        ok &= expect_true(d.publish_shared_state, "8053 failure publishes shared state");
        ok &= expect_true(d.send_details_update, "8053 failure sends details update");
    }

    // 7034 custom game runtime game_state advances through the runtime update queue.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 1u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        auto d = gbe::dota_lobby_state::compute_runtime_game_state_transition(
            lobby,
            true,
            true,
            3u,
            GBE_kDotaLaunchPhaseRunQueued,
            "custom game 7034 game_state");
        ok &= expect_true(d.queue_runtime_lobby_update, "7034 runtime queues update");
        ok &= expect_eq_u32(d.next_state, 2u, "7034 runtime next state");
        ok &= expect_eq_u32(d.next_game_state, 3u, "7034 runtime next game_state");
    }

    // 7034 launch poll emits a details-update intent except at terminal game_state 10.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;
        lobby.game_state = 0u;
        auto d = gbe::dota_lobby_state::compute_launch_poll_transition(lobby, "7034_launch_poll");
        ok &= expect_true(d.send_details_update, "7034 launch poll details intent");

        lobby.state = 2u;
        lobby.game_state = 10u;
        d = gbe::dota_lobby_state::compute_launch_poll_transition(lobby, "7034_launch_poll");
        ok &= expect_false(d.send_details_update, "7034 terminal state skips launch poll details");
    }

    return ok;
}

bool test_launch_lifecycle_action_sequence()
{
    bool ok = true;

    {
        gbe::dota_lifecycle::TransitionEffects effects;
        effects.transition.apply_lobby_state = true;
        effects.transition.next_state = 2u;
        effects.transition.next_game_state = 1u;
        effects.transition.mark_launch_phase = true;
        effects.transition.launch_phase = GBE_kDotaLaunchPhaseLoaded;
        effects.transition.publish_shared_state = true;
        effects.transition.send_details_update = true;
        effects.transition.reason = "8053_finished_loading";
        effects.local_steam_id = 700ull;
        effects.update_local_member_runtime = true;
        effects.publish_local_member_data = true;

        const GBE_DotaActionList actions = gbe::dota_lifecycle::build_transition_actions(effects);
        ok &= expect_eq_u64(actions.size(), 6u, "8053 action count");
        ok &= expect_true(actions[0].type == GBE_DotaActionType::LobbyStateApply, "8053 applies state first");
        ok &= expect_true(actions[1].type == GBE_DotaActionType::LobbyMemberRuntimeUpdate, "8053 updates member runtime second");
        ok &= expect_true(actions[2].type == GBE_DotaActionType::LaunchPhaseMark, "8053 marks launch phase third");
        ok &= expect_true(actions[3].type == GBE_DotaActionType::LobbyLocalMemberData, "8053 publishes local member fourth");
        ok &= expect_true(actions[4].type == GBE_DotaActionType::SharedLobbyPublish, "8053 publishes shared state fifth");
        ok &= expect_true(actions[5].type == GBE_DotaActionType::PracticeLobbyDetailsUpdate, "8053 sends details last");
    }

    {
        gbe::dota_lifecycle::TransitionEffects effects;
        effects.transition.apply_lobby_state = true;
        effects.transition.next_state = 2u;
        effects.transition.next_game_state = 0u;
        effects.transition.mark_launch_phase = true;
        effects.transition.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        effects.transition.queue_runtime_lobby_update = true;
        effects.transition.reason = "8052_started_loading";
        effects.trigger_emsg = 8052u;
        effects.source_job = 123ull;
        effects.runtime_update_note = "runtime packet after 8052";
        effects.fallback_publish_on_runtime_update_failure = true;

        const GBE_DotaActionList actions = gbe::dota_lifecycle::build_transition_actions(effects);
        ok &= expect_eq_u64(actions.size(), 3u, "8052 action count");
        ok &= expect_true(actions[0].type == GBE_DotaActionType::LaunchPhaseMark, "8052 marks launch phase first");
        ok &= expect_true(actions[1].type == GBE_DotaActionType::RuntimeLobbyDetailsUpdate, "8052 queues runtime update second");
        ok &= expect_true(actions[2].type == GBE_DotaActionType::SharedLobbyPublish, "8052 retains fallback shared publish");
        ok &= expect_true(actions[2].only_when_runtime_update_not_queued, "8052 fallback publish is conditional");
        ok &= expect_eq_u32(actions[1].emsg, 8052u, "8052 runtime emsg");
        ok &= expect_eq_u64(actions[1].job_id, 123ull, "8052 runtime source job");
    }

    {
        const GBE_DotaActionList actions = gbe::dota_lifecycle::build_member_runtime_actions(
            700ull,
            false,
            0u,
            false,
            "7034_disconnected_player");
        ok &= expect_eq_u64(actions.size(), 2u, "7034 member action count");
        ok &= expect_true(actions[0].type == GBE_DotaActionType::LobbyMemberRuntimeUpdate, "7034 member mutation first");
        ok &= expect_false(actions[0].connected, "7034 disconnected state preserved");
        ok &= expect_true(actions[1].type == GBE_DotaActionType::SharedLobbyPublish, "7034 member publish second");
        ok &= expect_true(actions[1].only_when_previous_action_succeeded, "7034 publish requires mutation");
    }

    {
        gbe::dota_lifecycle::TransitionEffects effects;
        effects.transition.send_details_update = true;
        effects.transition.reason = "7034_launch_poll";
        const GBE_DotaActionList actions = gbe::dota_lifecycle::build_transition_actions(effects);
        ok &= expect_eq_u64(actions.size(), 1u, "7034 poll action count");
        ok &= expect_true(actions[0].type == GBE_DotaActionType::PracticeLobbyDetailsUpdate, "7034 poll sends details");
    }

    return ok;
}

bool test_lifecycle_action_properties()
{
    bool ok = true;

    for (unsigned int mask = 0u; mask < 256u; ++mask) {
        gbe::dota_lifecycle::TransitionEffects effects;
        effects.transition.apply_lobby_state = (mask & 1u) != 0u;
        effects.transition.next_state = 2u;
        effects.transition.next_game_state = 3u;
        effects.update_local_member_runtime = (mask & 2u) != 0u;
        effects.local_steam_id = effects.update_local_member_runtime ? 700ull : 0ull;
        effects.transition.mark_launch_phase = (mask & 4u) != 0u;
        effects.transition.launch_phase = GBE_kDotaLaunchPhaseLoaded;
        effects.transition.queue_runtime_lobby_update = (mask & 8u) != 0u;
        effects.publish_local_member_data = (mask & 16u) != 0u;
        effects.transition.publish_shared_state = (mask & 32u) != 0u;
        effects.transition.send_details_update = (mask & 64u) != 0u;
        effects.fallback_publish_on_runtime_update_failure = (mask & 128u) != 0u;
        effects.transition.runtime_update_delay = 0.25;
        effects.transition.reason = "P5_property";
        effects.trigger_emsg = 7034u;
        effects.source_job = 123ull;
        effects.runtime_update_note = "P5_runtime";

        const GBE_DotaActionList first = gbe::dota_lifecycle::build_transition_actions(effects);
        const GBE_DotaActionList second = gbe::dota_lifecycle::build_transition_actions(effects);
        ok &= expect_true(
            lifecycle_action_fingerprint(first) == lifecycle_action_fingerprint(second),
            "P5-A identical effects produce identical action sequences");

        std::size_t runtime_index = first.size();
        std::size_t local_publish_index = first.size();
        std::size_t shared_publish_index = first.size();
        std::size_t details_index = first.size();
        for (std::size_t i = 0; i < first.size(); ++i) {
            switch (first[i].type) {
                case GBE_DotaActionType::RuntimeLobbyDetailsUpdate: runtime_index = i; break;
                case GBE_DotaActionType::LobbyLocalMemberData: local_publish_index = i; break;
                case GBE_DotaActionType::SharedLobbyPublish: shared_publish_index = i; break;
                case GBE_DotaActionType::PracticeLobbyDetailsUpdate: details_index = i; break;
                default: break;
            }
        }
        if (runtime_index < first.size() && local_publish_index < first.size())
            ok &= expect_true(runtime_index < local_publish_index, "P5-B runtime precedes local publish");
        if (local_publish_index < first.size() && shared_publish_index < first.size())
            ok &= expect_true(local_publish_index < shared_publish_index, "P5-B local publish precedes shared publish");
        if (shared_publish_index < first.size() && details_index < first.size())
            ok &= expect_true(shared_publish_index < details_index, "P5-B shared publish precedes details update");
    }

    const gbe::dota_lifecycle::TransitionEffects empty_effects;
    ok &= expect_true(
        gbe::dota_lifecycle::build_transition_actions(empty_effects).empty(),
        "P5-C empty effects produce no actions");
    return ok;
}

bool test_lobby_generation_allocation_rules()
{
    using gbe::dota_lobby_generation::Boundary;
    using gbe::dota_lobby_generation::Counter;
    using gbe::dota_lobby_generation::Generation;

    bool ok = true;
    Counter counter;
    ok &= expect_false(counter.current().assigned(), "unallocated generation starts at zero");

    const Boundary boundaries[] = {
        Boundary::Create,
        Boundary::Join,
        Boundary::Leave,
        Boundary::Reset,
        Boundary::Recover,
    };
    std::uint64_t expected = 0u;
    for (Boundary boundary : boundaries) {
        const auto result = counter.advance(boundary);
        ++expected;
        ok &= expect_true(result.advanced, "lifecycle boundary advances generation");
        ok &= expect_eq_u64(result.previous.value, expected - 1u, "generation result preserves previous value");
        ok &= expect_eq_u64(result.current.value, expected, "generation increments exactly once per boundary");
        ok &= expect_true(result.boundary == boundary, "generation result preserves boundary reason");
        ok &= expect_true(
            gbe::dota_lobby_generation::is_newer(result.current, result.previous),
            "advanced generation is strictly newer");
    }

    const Generation maximum{std::numeric_limits<std::uint64_t>::max()};
    Counter exhausted(maximum);
    const auto overflow = exhausted.advance(Boundary::Create);
    ok &= expect_false(overflow.advanced, "maximum generation refuses allocation");
    ok &= expect_eq_u64(overflow.previous.value, maximum.value, "overflow preserves previous generation");
    ok &= expect_eq_u64(overflow.current.value, maximum.value, "overflow never wraps generation");
    ok &= expect_false(
        gbe::dota_lobby_generation::is_newer(Generation{}, maximum),
        "wrapped zero is never newer than maximum generation");
    return ok;
}

bool test_lobby_generation_properties()
{
    using gbe::dota_lobby_generation::Boundary;
    using gbe::dota_lobby_generation::Counter;
    using gbe::dota_lobby_generation::Generation;

    const Boundary boundaries[] = {
        Boundary::Create,
        Boundary::Join,
        Boundary::Leave,
        Boundary::Reset,
        Boundary::Recover,
    };
    bool ok = true;
    for (std::uint64_t seed = 1u; seed <= 64u; ++seed) {
        Counter counter(Generation{seed * 1024u});
        for (std::uint64_t step = 0u; step < 32u; ++step) {
            const Generation previous = counter.current();
            const Boundary boundary = boundaries[(seed + step) % 5u];
            const auto result = counter.advance(boundary);
            ok &= expect_true(result.advanced, "P6-A lifecycle generation advances before exhaustion");
            ok &= expect_eq_u64(result.previous.value, previous.value, "P6-A advance reports the prior generation");
            ok &= expect_eq_u64(result.current.value, previous.value + 1u, "P6-A new lifecycle generation changes strictly by one");
            ok &= expect_true(
                gbe::dota_lobby_generation::is_newer(result.current, result.previous),
                "P6-A new lifecycle generation is strictly newer");
        }
    }
    return ok;
}

// ---- Category 2: stale generic lobby state regression ----------------------

bool test_stale_generic_lobby_state_regression()
{
    bool ok = true;

    // adopt_shared_lobby_to_local: normalize_custom_readyup_run_state collapses a stale READYUP (4)
    // snapshot to RUN (2) when a custom game has progressed past game_state>=2.
    {
        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.generation = 17u;
        shared.custom_game.game_id = 500ull;
        shared.state = 4u;  // READYUP
        shared.game_state = 2u;  // in-game
        GBE_LocalLobby local{};
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, true, local);
        ok &= expect_eq_u32(local.state, 2u, "adopt normalizes stale readyup to run for custom game");
        ok &= expect_eq_u32(local.game_state, 2u, "adopt keeps game_state");
        ok &= expect_eq_u64(local.generation, shared.generation, "adopt preserves lobby generation");
    }

    // adopt_shared_lobby_to_local: no normalization for non-custom lobbies.
    {
        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.state = 4u;
        shared.game_state = 2u;
        GBE_LocalLobby local{};
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, true, local);
        ok &= expect_eq_u32(local.state, 4u, "adopt keeps readyup for non-custom game");
    }

    // adopt_shared_lobby_to_local: no normalization when game_state < 2.
    {
        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.custom_game.game_id = 500ull;
        shared.state = 4u;
        shared.game_state = 1u;  // pre-game
        GBE_LocalLobby local{};
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, true, local);
        ok &= expect_eq_u32(local.state, 4u, "adopt keeps readyup when game_state below threshold");
    }

    // adopt_shared_lobby_to_local: no normalization when state != 4.
    {
        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.custom_game.game_id = 500ull;
        shared.state = 1u;  // SERVERSETUP, not READYUP
        shared.game_state = 2u;
        GBE_LocalLobby local{};
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, true, local);
        ok &= expect_eq_u32(local.state, 1u, "adopt keeps non-readyup state");
    }

    // adopt_shared_lobby_to_local: clear_server_id_without_match clears server_id when match_id==0.
    {
        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.server_id = 700ull;
        shared.match_id = 0ull;
        GBE_LocalLobby local{};
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, true, false, local);
        ok &= expect_eq_u64(local.server_id, 0ull, "adopt clears server_id without match");
    }

    // adopt_shared_lobby_to_local: clear_server_id_without_match keeps server_id when match_id!=0.
    {
        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.server_id = 700ull;
        shared.match_id = 500ull;
        GBE_LocalLobby local{};
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, true, false, local);
        ok &= expect_eq_u64(local.server_id, 700ull, "adopt keeps server_id with match");
    }

    // adopt_shared_lobby_to_local: same-generation chat/broadcast local applies keep Local channel groups.
    {
        GBE_LocalLobby local = make_active_lobby();
        local.generation = 33ull;
        gbe::dota_lobby_state::apply_chat_channel(local, 42ull, "local-chat", 3u);
        gbe::dota_lobby_state::apply_broadcast_channel(local, 7u, "US", "local-cast", "en");

        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.generation = local.generation;
        shared.has_chat_channel = true;
        shared.chat_channel_id = 43ull;
        shared.chat_channel_name = "shared-chat";
        shared.chat_channel_type = 4u;
        shared.has_broadcast_channel = true;
        shared.broadcast_channel_id = 8u;
        shared.broadcast_country_code = "GB";
        shared.broadcast_description = "shared-cast";
        shared.broadcast_language_code = "fr";
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, false, local);

        ok &= expect_true(local.has_chat_channel, "same-generation adopt keeps local chat present");
        ok &= expect_eq_u64(local.chat_channel_id, 42ull, "same-generation adopt keeps local chat id");
        ok &= expect_eq_str(local.chat_channel_name, "local-chat", "same-generation adopt keeps local chat name");
        ok &= expect_eq_u32(local.chat_channel_type, 3u, "same-generation adopt keeps local chat type");
        ok &= expect_true(local.has_broadcast_channel, "same-generation adopt keeps local broadcast present");
        ok &= expect_eq_u32(local.broadcast_channel_id, 7u, "same-generation adopt keeps local broadcast id");
        ok &= expect_eq_str(local.broadcast_country_code, "US", "same-generation adopt keeps local broadcast country");
        ok &= expect_eq_str(local.broadcast_description, "local-cast", "same-generation adopt keeps local broadcast description");
        ok &= expect_eq_str(local.broadcast_language_code, "en", "same-generation adopt keeps local broadcast language");
    }

    // adopt_shared_lobby_to_local: older local chat/broadcast source markers accept shared channel groups.
    {
        GBE_LocalLobby local = make_active_lobby();
        local.generation = 33ull;
        gbe::dota_lobby_state::apply_chat_channel(local, 42ull, "local-chat", 3u);
        gbe::dota_lobby_state::apply_broadcast_channel(local, 7u, "US", "local-cast", "en");

        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.generation = local.generation + 1ull;
        shared.has_chat_channel = true;
        shared.chat_channel_id = 43ull;
        shared.chat_channel_name = "shared-chat";
        shared.chat_channel_type = 4u;
        shared.has_broadcast_channel = true;
        shared.broadcast_channel_id = 8u;
        shared.broadcast_country_code = "GB";
        shared.broadcast_description = "shared-cast";
        shared.broadcast_language_code = "fr";
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, false, local);

        ok &= expect_eq_u64(local.chat_channel_id, 43ull, "newer shared adopt applies chat id");
        ok &= expect_eq_str(local.chat_channel_name, "shared-chat", "newer shared adopt applies chat name");
        ok &= expect_eq_u32(local.broadcast_channel_id, 8u, "newer shared adopt applies broadcast id");
        ok &= expect_eq_str(local.broadcast_description, "shared-cast", "newer shared adopt applies broadcast description");
    }

    // adopt_shared_lobby_to_local: same-generation owner name local apply keeps Local owner name.
    {
        GBE_LocalLobby local = make_active_lobby();
        local.generation = 44ull;
        gbe::dota_lobby_state::apply_lobby_owner_name(local, "local-owner");

        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.generation = local.generation;
        shared.owner_name = "shared-owner";
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, false, local);

        ok &= expect_eq_str(local.owner_name, "local-owner", "same-generation adopt keeps local owner name");
    }

    // adopt_shared_lobby_to_local: older owner name source marker accepts shared owner name.
    {
        GBE_LocalLobby local = make_active_lobby();
        local.generation = 44ull;
        gbe::dota_lobby_state::apply_lobby_owner_name(local, "local-owner");

        GBE_SharedDotaLobbyState shared{};
        shared.active = true;
        shared.generation = local.generation + 1ull;
        shared.owner_name = "shared-owner";
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, false, local);

        ok &= expect_eq_str(local.owner_name, "shared-owner", "newer shared adopt applies owner name");
    }

    // compose_queued_lobby_state_apply_plan: preserve_monotonic_game_state prevents stale reset.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.generation = 23u;
        lobby.state = 2u;
        lobby.game_state = 3u;  // current in-game progress
        auto plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
            lobby, 2u, 0u, true, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_true(plan.preserved_game_state, "queued apply preserves monotonic game_state");
        ok &= expect_eq_u32(plan.game_state, 3u, "queued apply kept current game_state");
    }

    // compose_queued_lobby_state_apply_plan: no preservation when current game_state is zero.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 0u;
        auto plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
            lobby, 2u, 0u, true, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_false(plan.preserved_game_state, "queued apply no preserve when current zero");
    }

    // compose_queued_lobby_state_apply_plan: no preservation when current state != 2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;  // not RUN
        lobby.game_state = 3u;
        auto plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
            lobby, 2u, 0u, true, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_false(plan.preserved_game_state, "queued apply no preserve when current state not run");
    }

    // compose_queued_lobby_state_apply_plan: no preservation when flag disabled.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 3u;
        auto plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
            lobby, 2u, 0u, false, GBE_kDotaLaunchPhaseSetupSynced, GBE_kDotaLaunchPhaseRunQueued);
        ok &= expect_false(plan.preserved_game_state, "queued apply no preserve when flag disabled");
        ok &= expect_eq_u32(plan.game_state, 0u, "queued apply resets game_state when flag disabled");
    }

    // apply_queued_lobby_state_apply_plan: apply the computed launch/runtime field group together.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;
        lobby.game_state = 2u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
        const gbe::dota_lobby_state::QueuedLobbyStateApplyPlan plan{
            2u,
            3u,
            GBE_kDotaLaunchPhaseRunQueued,
            false,
        };
        gbe::dota_lobby_state::apply_queued_lobby_state_apply_plan(lobby, plan);
        ok &= expect_eq_u32(lobby.state, 2u, "queued apply boundary updates state");
        ok &= expect_eq_u32(lobby.game_state, 3u, "queued apply boundary updates game_state");
        ok &= expect_eq_u32(lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "queued apply boundary updates launch phase");
    }

    return ok;
}

// ---- Category 3: owner disconnect and reconnect ----------------------------

bool test_owner_disconnect_and_reconnect()
{
    bool ok = true;

    // compute_abandon_decision: treat_as_current_game_disconnect true when server owner connected
    // with an allocated server (server_id != 0) even before game_state advances.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 0u;
        lobby.server_id = 100ull;
        lobby.owner_connected = true;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_true(d.treat_as_current_game_disconnect, "current-game disconnect with server allocated");
        ok &= expect_false(d.ready_for_abandon_teardown, "not ready for teardown at game_state 0 on direct server");
        ok &= expect_true(d.queue_cache_unsubscribed, "current-game disconnect queues 25");
        ok &= expect_true(d.set_pending_reset_after_cache_unsubscribed, "current-game disconnect marks pending reset");
        ok &= expect_false(d.queue_postgame_teardown, "current-game disconnect skips postgame teardown");
    }

    // compute_abandon_decision: treat_as_current_game_disconnect true when game_state>=1 and owner connected.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 1u;
        lobby.server_id = 0ull;
        lobby.owner_connected = true;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_true(d.treat_as_current_game_disconnect, "current-game disconnect with game_state>=1");
    }

    // compute_abandon_decision: not a current-game disconnect when owner disconnected (falls through to teardown).
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.server_id = 100ull;
        lobby.owner_connected = false;  // owner gone
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_false(d.treat_as_current_game_disconnect, "no current-game disconnect when owner disconnected");
        ok &= expect_true(d.ready_for_abandon_teardown, "ready for teardown when owner disconnected and game_state>=2");
        ok &= expect_true(d.queue_postgame_teardown, "ready abandon queues postgame teardown");
        ok &= expect_true(d.discard_queued_launch_messages, "ready abandon discards queued launch messages");
        ok &= expect_true(d.suppress_abandoned_lobby, "ready abandon suppresses abandoned lobby");
        ok &= expect_true(d.push_postgame_cache_unsubscribed, "ready abandon pushes 25 through teardown");
        ok &= expect_true(d.push_postgame_join, "ready abandon pushes postgame join through teardown");
    }

    // compute_abandon_decision: not a current-game disconnect on client (non-server) even if owner connected.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 1u;
        lobby.server_id = 100ull;
        lobby.owner_connected = true;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, false);  // client
        ok &= expect_false(d.treat_as_current_game_disconnect, "no current-game disconnect on client");
    }

    // compute_abandon_decision: not a current-game disconnect when state != 2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;  // SERVERSETUP, not RUN
        lobby.game_state = 1u;
        lobby.server_id = 100ull;
        lobby.owner_connected = true;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_false(d.treat_as_current_game_disconnect, "no current-game disconnect when state not run");
    }

    // build_reconnect_context: valid reconnect when game started and server/connect known.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.server_id = 700ull;
        lobby.connect = "1.2.3.4:27015";
        lobby.owner_steam_id = 42ull;
        GBE_DotaReconnectContext ctx{};
        const auto source = gbe::dota_reconnect::source_from_local_lobby(lobby);
        bool result = gbe::dota_reconnect::build_context(source, ctx) == gbe::dota_reconnect::RejectReason::None;
        ok &= expect_true(result, "reconnect context built for started game");
        ok &= expect_eq_u64(ctx.generation, lobby.generation, "reconnect generation");
        ok &= expect_eq_u64(ctx.lobby_id, lobby.lobby_id, "reconnect lobby_id");
        ok &= expect_eq_u64(ctx.server_id, 700ull, "reconnect server_id");
        ok &= expect_eq_u32(ctx.lobby_state, 2u, "reconnect lobby_state");
        ok &= expect_eq_u32(ctx.game_state, 2u, "reconnect game_state");
        ok &= expect_eq_u64(ctx.owner_steam_id, 42ull, "reconnect owner_steam_id");
        ok &= expect_eq_str(std::string(ctx.connect), "1.2.3.4:27015", "reconnect connect endpoint");
    }

    // build_reconnect_context: rejected when lobby inactive.
    {
        GBE_LocalLobby lobby{};
        lobby.active = false;
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.server_id = 700ull;
        lobby.connect = "1.2.3.4:27015";
        GBE_DotaReconnectContext ctx{};
        const auto source = gbe::dota_reconnect::source_from_local_lobby(lobby);
        ok &= expect_false(gbe::dota_reconnect::build_context(source, ctx) == gbe::dota_reconnect::RejectReason::None, "reconnect rejected when inactive");
    }

    // build_reconnect_context: rejected when game not started (state<2 AND game_state<2).
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;
        lobby.game_state = 1u;  // both below 2
        lobby.server_id = 700ull;
        lobby.connect = "1.2.3.4:27015";
        GBE_DotaReconnectContext ctx{};
        const auto source = gbe::dota_reconnect::source_from_local_lobby(lobby);
        ok &= expect_false(gbe::dota_reconnect::build_context(source, ctx) == gbe::dota_reconnect::RejectReason::None, "reconnect rejected when game not started");
    }

    // build_reconnect_context: accepted when game_state>=2 even if state<2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;
        lobby.game_state = 2u;  // game_state carries the start condition
        lobby.server_id = 700ull;
        lobby.connect = "1.2.3.4:27015";
        GBE_DotaReconnectContext ctx{};
        const auto source = gbe::dota_reconnect::source_from_local_lobby(lobby);
        ok &= expect_true(gbe::dota_reconnect::build_context(source, ctx) == gbe::dota_reconnect::RejectReason::None, "reconnect accepted via game_state>=2");
    }

    // build_reconnect_context: rejected when server_id is zero.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.server_id = 0ull;
        lobby.connect = "1.2.3.4:27015";
        GBE_DotaReconnectContext ctx{};
        const auto source = gbe::dota_reconnect::source_from_local_lobby(lobby);
        ok &= expect_false(gbe::dota_reconnect::build_context(source, ctx) == gbe::dota_reconnect::RejectReason::None, "reconnect rejected when server_id zero");
    }

    // build_reconnect_context: rejected when connect empty.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.server_id = 700ull;
        lobby.connect = "";
        GBE_DotaReconnectContext ctx{};
        const auto source = gbe::dota_reconnect::source_from_local_lobby(lobby);
        ok &= expect_false(gbe::dota_reconnect::build_context(source, ctx) == gbe::dota_reconnect::RejectReason::None, "reconnect rejected when connect empty");
    }

    return ok;
}

bool test_reconnect_eligibility_decision()
{
    bool ok = true;

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            true,
            true,
            2u,
            2u,
            700ull,
            true,
            500ull,
            true,
            GBE_kDotaLaunchPhaseLoaded);
        ok &= expect_true(d.context_eligible, "reconnect shared context eligible");
        ok &= expect_true(d.custom_game, "reconnect custom game flagged");
        ok &= expect_true(d.owner_connected, "reconnect owner connected recorded");
        ok &= expect_true(d.launch_run_or_later, "reconnect launch run-or-later recorded");
        ok &= expect_true(d.launch_loaded, "reconnect launch loaded recorded");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            false,
            true,
            2u,
            2u,
            700ull,
            true,
            0ull,
            false,
            0u);
        ok &= expect_false(d.context_eligible, "reconnect rejects invalid source");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            true,
            false,
            2u,
            2u,
            700ull,
            true,
            0ull,
            false,
            0u);
        ok &= expect_false(d.context_eligible, "reconnect rejects inactive source");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            true,
            true,
            1u,
            1u,
            700ull,
            true,
            0ull,
            false,
            0u);
        ok &= expect_false(d.started, "reconnect not started below thresholds");
        ok &= expect_false(d.context_eligible, "reconnect rejects not-started source");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            true,
            true,
            1u,
            2u,
            700ull,
            true,
            0ull,
            false,
            0u);
        ok &= expect_true(d.started, "reconnect game_state threshold starts context");
        ok &= expect_true(d.context_eligible, "reconnect accepts started game_state");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            true,
            true,
            2u,
            2u,
            0ull,
            true,
            0ull,
            false,
            0u);
        ok &= expect_false(d.has_server_id, "reconnect missing server id recorded");
        ok &= expect_false(d.context_eligible, "reconnect rejects missing server id");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            true,
            true,
            2u,
            2u,
            700ull,
            false,
            0ull,
            false,
            0u);
        ok &= expect_false(d.has_connect, "reconnect missing connect recorded");
        ok &= expect_false(d.context_eligible, "reconnect rejects missing connect");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
            true,
            true,
            2u,
            2u,
            700ull,
            true,
            500ull,
            false,
            GBE_kDotaLaunchPhaseSetupSynced);
        ok &= expect_true(d.context_eligible, "reconnect keeps existing payload eligibility before run phase");
        ok &= expect_false(d.launch_run_or_later, "reconnect launch before run recorded");
        ok &= expect_false(d.launch_loaded, "reconnect launch before loaded recorded");
    }

    return ok;
}

bool test_reconnect_context_source_pipeline()
{
    using gbe::dota_reconnect::RejectReason;
    using gbe::dota_reconnect::Source;
    using gbe::dota_reconnect::SourceKind;

    bool ok = true;
    auto make_source = [](SourceKind kind, std::uint64_t seed) {
        Source source{};
        source.kind = kind;
        source.valid = true;
        source.active = true;
        source.generation = seed;
        source.lobby_id = seed + 1u;
        source.lobby_state = 2u;
        source.game_state = 2u;
        source.server_id = seed + 2u;
        source.custom_game_id = seed + 3u;
        source.owner_steam_id = seed + 4u;
        source.connect = "10.0.0." + std::to_string(seed % 200u + 1u) + ":27015 10.0.1.1:27016";
        return source;
    };

    const Source shared = make_source(SourceKind::Shared, 100u);
    const Source recent = make_source(SourceKind::Recent, 200u);
    const Source local = make_source(SourceKind::Local, 300u);
    const Source generic = make_source(SourceKind::GenericRecovery, 400u);

    for (const Source &source : {shared, recent, local, generic}) {
        GBE_DotaReconnectContext context{};
        ok &= expect_true(
            gbe::dota_reconnect::build_context(source, context) == RejectReason::None,
            "each reconnect source kind builds independently");
        ok &= expect_eq_u64(context.generation, source.generation, "builder propagates reconnect generation");
        ok &= expect_eq_u64(context.lobby_id, source.lobby_id, "builder propagates reconnect lobby id");
        ok &= expect_eq_u64(context.server_id, source.server_id, "builder propagates reconnect server id");
        ok &= expect_eq_u64(context.custom_game_id, source.custom_game_id, "builder propagates reconnect custom game id");
        ok &= expect_eq_u64(context.owner_steam_id, source.owner_steam_id, "builder propagates reconnect owner");
        ok &= expect_false(std::string(context.connect).empty(), "valid reconnect output has endpoint");
        ok &= expect_true(std::string(context.connect).find(' ') == std::string::npos, "reconnect endpoint is normalized");
    }

    auto selection = gbe::dota_reconnect::select_context({generic, local, recent, shared});
    ok &= expect_true(selection.selected, "source matrix selects a context");
    ok &= expect_true(selection.source_kind == SourceKind::Shared, "shared source has stable highest priority");
    ok &= expect_eq_u64(selection.context.generation, shared.generation, "selected generation comes from shared source");
    ok &= expect_eq_u64(selection.context.lobby_id, shared.lobby_id, "selected fields come from shared source");
    ok &= expect_eq_u64(selection.context.server_id, shared.server_id, "selected server comes from shared source");
    ok &= expect_eq_u64(selection.context.owner_steam_id, shared.owner_steam_id, "selected owner comes from shared source");

    selection = gbe::dota_reconnect::select_context({recent, generic, local, shared});
    ok &= expect_true(selection.source_kind == SourceKind::Shared, "source priority is independent of input order");

    Source invalid_shared = shared;
    invalid_shared.active = false;
    selection = gbe::dota_reconnect::select_context({generic, invalid_shared, recent});
    ok &= expect_true(selection.selected, "invalid high-priority source falls back");
    ok &= expect_true(selection.source_kind == SourceKind::Recent, "recent source precedes generic recovery");
    ok &= expect_eq_u64(selection.context.generation, recent.generation, "fallback generation uses recent source");
    ok &= expect_eq_u64(selection.context.lobby_id, recent.lobby_id, "fallback output uses one recent source");

    GBE_DotaReconnectContext rejected_context{};
    Source rejected = shared;
    rejected.active = false;
    ok &= expect_true(
        gbe::dota_reconnect::build_context(rejected, rejected_context) == RejectReason::Inactive,
        "builder reports inactive source");
    rejected = shared;
    rejected.lobby_state = 1u;
    rejected.game_state = 1u;
    ok &= expect_true(
        gbe::dota_reconnect::build_context(rejected, rejected_context) == RejectReason::GameNotStarted,
        "builder reports game not started");
    rejected = shared;
    rejected.server_id = 0u;
    ok &= expect_true(
        gbe::dota_reconnect::build_context(rejected, rejected_context) == RejectReason::MissingServerId,
        "builder reports missing server");
    rejected = shared;
    rejected.connect.clear();
    ok &= expect_true(
        gbe::dota_reconnect::build_context(rejected, rejected_context) == RejectReason::MissingEndpoint,
        "builder reports missing endpoint");

    GBE_LocalLobby generic_lobby = make_active_lobby();
    generic_lobby.state = 2u;
    generic_lobby.game_state = 2u;
    generic_lobby.generation = 37u;
    generic_lobby.server_id = 700u;
    generic_lobby.connect = "10.1.1.1:27015";
    generic_lobby.custom_game.game_id = 500u;
    generic_lobby.owner_steam_id = 42u;
    generic_lobby.members.push_back(GBE_DotaLobbyMemberState{});
    generic_lobby.members.back().steam_id = 84u;
    Source generic_source = gbe::dota_reconnect::source_from_generic_lobby(generic_lobby, 84u);
    ok &= expect_true(generic_source.valid, "generic recovery accepts local lobby member");
    ok &= expect_eq_u64(generic_source.generation, generic_lobby.generation, "generic recovery preserves generation");
    ok &= expect_true(
        gbe::dota_reconnect::build_context(generic_source, rejected_context) == RejectReason::None,
        "generic recovery uses common builder");
    ok &= expect_false(
        gbe::dota_reconnect::source_from_generic_lobby(generic_lobby, 42u).valid,
        "generic recovery excludes local owner");
    generic_lobby.custom_game.game_id = 0u;
    ok &= expect_false(
        gbe::dota_reconnect::source_from_generic_lobby(generic_lobby, 84u).valid,
        "generic recovery excludes ordinary practice lobby");

    return ok;
}

bool test_reconnect_interception_decision()
{
    bool ok = true;

    GBE_DotaReconnectContext ctx{};
    ctx.server_id = 700ull;
    ctx.lobby_state = 2u;
    ctx.game_state = 2u;
    ctx.custom_game_id = 500ull;
    std::strncpy(ctx.connect, "1.2.3.4:27015", sizeof(ctx.connect) - 1);
    ctx.owner_steam_id = 42ull;

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_interception_decision(
            ctx,
            true,
            100ull,
            700ull,
            true);
        ok &= expect_true(d.remote_matches_server, "intercept remote server match");
        ok &= expect_true(d.state_ready, "intercept state ready");
        ok &= expect_true(d.has_connect, "intercept has connect");
        ok &= expect_true(d.arcade_context, "intercept arcade context");
        ok &= expect_false(d.local_is_owner, "intercept remote player is not owner");
        ok &= expect_true(d.p2p_rendezvous_candidate, "intercept attempts p2p rendezvous");
        ok &= expect_true(d.can_post_connection_state, "intercept can post connection state");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_interception_decision(
            ctx,
            true,
            42ull,
            700ull,
            true);
        ok &= expect_true(d.local_is_owner, "intercept owner detected");
        ok &= expect_false(d.p2p_rendezvous_candidate, "intercept owner skips p2p rendezvous");
        ok &= expect_false(d.can_post_connection_state, "intercept owner skips post connection state");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_interception_decision(
            ctx,
            true,
            100ull,
            123ull,
            true);
        ok &= expect_false(d.remote_matches_server, "intercept remote mismatch recorded");
        ok &= expect_false(d.p2p_rendezvous_candidate, "intercept remote mismatch skips p2p rendezvous");
        ok &= expect_true(d.can_post_connection_state, "intercept post connection does not require remote id");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_interception_decision(
            ctx,
            true,
            100ull,
            700ull,
            false);
        ok &= expect_false(d.reconnect_eligible, "intercept eligibility flag recorded");
        ok &= expect_true(d.p2p_rendezvous_candidate, "intercept p2p candidate keeps atomic eligibility behavior");
        ok &= expect_false(d.can_post_connection_state, "intercept ineligible skips post connection state");
    }

    {
        GBE_DotaReconnectContext non_arcade = ctx;
        non_arcade.custom_game_id = 0ull;
        auto d = gbe::dota_lobby_state::compute_reconnect_interception_decision(
            non_arcade,
            true,
            100ull,
            700ull,
            true);
        ok &= expect_false(d.arcade_context, "intercept non-arcade recorded");
        ok &= expect_true(d.p2p_rendezvous_candidate, "intercept p2p keeps existing non-arcade behavior");
        ok &= expect_false(d.can_post_connection_state, "intercept post connection requires arcade context");
    }

    {
        GBE_DotaReconnectContext missing_connect = ctx;
        missing_connect.connect[0] = '\0';
        auto d = gbe::dota_lobby_state::compute_reconnect_interception_decision(
            missing_connect,
            true,
            100ull,
            700ull,
            true);
        ok &= expect_false(d.has_connect, "intercept missing connect recorded");
        ok &= expect_false(d.p2p_rendezvous_candidate, "intercept missing connect skips p2p rendezvous");
        ok &= expect_false(d.can_post_connection_state, "intercept missing connect skips post connection state");
    }

    {
        auto d = gbe::dota_lobby_state::compute_reconnect_interception_decision(
            ctx,
            false,
            100ull,
            700ull,
            true);
        ok &= expect_false(d.has_context, "intercept missing context recorded");
        ok &= expect_false(d.p2p_rendezvous_candidate, "intercept missing context skips p2p rendezvous");
        ok &= expect_false(d.can_post_connection_state, "intercept missing context skips post connection state");
    }

    return ok;
}

bool test_runtime_reset_reconnect_preserve_decision()
{
    bool ok = true;

    {
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision(
            gbe::dota_diagnostic::Reason::DisconnectCurrentGameAfterCacheUnsubscribed);
        ok &= expect_true(d.preserve_reconnect_context, "7035 current-game disconnect reset preserves reconnect context");
    }

    {
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision(gbe::dota_diagnostic::Reason::Unknown);
        ok &= expect_false(d.preserve_reconnect_context, "shutdown reset clears reconnect context");
    }

    {
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision(gbe::dota_diagnostic::Reason::Unknown);
        ok &= expect_false(d.preserve_reconnect_context, "ordinary leave reset clears reconnect context");
    }

    {
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision(gbe::dota_diagnostic::Reason::Unknown);
        ok &= expect_false(d.preserve_reconnect_context, "unknown reset clears reconnect context");
    }

    return ok;
}

// ---- Category 4: post-game teardown suppression ---------------------------

bool test_post_game_teardown_suppression()
{
    bool ok = true;

    // 7035 request context captures wrapped/session/server and local lobby runtime state.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 1u;
        lobby.server_id = 700ull;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        lobby.chat_channel_id = 7014ull;
        lobby.owner_connected = true;
        lobby.custom_game.game_id = 500ull;

        gbe::dota_lobby_state::DotaAbandonRequestContext context{};
        ok &= expect_true(gbe::dota_lobby_state::build_dota_abandon_request_context(lobby, true, true, true, context), "7035 context builds for active lobby");
        ok &= expect_true(context.wrapped, "7035 context wrapped");
        ok &= expect_true(context.has_wrapped_session, "7035 context session");
        ok &= expect_true(context.is_server, "7035 context server");
        ok &= expect_true(context.owner_connected, "7035 context owner connected");
        ok &= expect_true(context.has_custom_game_details, "7035 context custom game");
        ok &= expect_eq_u64(context.lobby_id, 100ull, "7035 context lobby id");
        ok &= expect_eq_u32(context.lobby_state, 2u, "7035 context lobby state");
        ok &= expect_eq_u32(context.game_state, 1u, "7035 context game state");
        ok &= expect_eq_u64(context.server_id, 700ull, "7035 context server id");
        ok &= expect_eq_u32(context.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "7035 context launch phase");
        ok &= expect_eq_u64(context.pre_postgame_chat_channel_id, 7014ull, "7035 context chat channel");

        auto d = gbe::dota_lobby_state::compute_abandon_decision(context);
        ok &= expect_true(d.ready_for_abandon_teardown, "7035 context decision ready with wrapped session");
        ok &= expect_false(d.require_wrapped_session, "7035 context decision has wrapped session");

        context.has_wrapped_session = false;
        d = gbe::dota_lobby_state::compute_abandon_decision(context);
        ok &= expect_true(d.require_wrapped_session, "7035 context decision requires missing wrapped session");
    }

    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.active = false;
        gbe::dota_lobby_state::DotaAbandonRequestContext context{};
        ok &= expect_false(gbe::dota_lobby_state::build_dota_abandon_request_context(lobby, false, false, true, context), "7035 context skips inactive lobby");
    }

    // Threshold: wrapped/client uses 1, direct server uses 2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 1u;
        auto d_wrapped_server = gbe::dota_lobby_state::compute_abandon_decision(lobby, true, true);
        ok &= expect_eq_u32(d_wrapped_server.abandon_game_state_threshold, 1u, "threshold 1 for wrapped server");
        ok &= expect_true(d_wrapped_server.ready_for_abandon_teardown, "wrapped can abandon at game_state 1");

        auto d_direct_server = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_eq_u32(d_direct_server.abandon_game_state_threshold, 2u, "threshold 2 for direct server");
        ok &= expect_false(d_direct_server.ready_for_abandon_teardown, "direct server cannot abandon at game_state 1");

        auto d_direct_client = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, false);
        ok &= expect_eq_u32(d_direct_client.abandon_game_state_threshold, 1u, "threshold 1 for direct client");
        ok &= expect_true(d_direct_client.ready_for_abandon_teardown, "direct client can abandon at game_state 1");

        auto d_wrapped_client = gbe::dota_lobby_state::compute_abandon_decision(lobby, true, false);
        ok &= expect_eq_u32(d_wrapped_client.abandon_game_state_threshold, 1u, "threshold 1 for wrapped client");
    }

    // ready_for_abandon_teardown: direct server reaches it at game_state>=2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_true(d.ready_for_abandon_teardown, "direct server ready at game_state 2");
    }

    // ready_for_abandon_teardown: false when state != 2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;  // SERVERSETUP
        lobby.game_state = 2u;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, true, true);
        ok &= expect_false(d.ready_for_abandon_teardown, "not ready when state not run");
    }

    // ready_for_abandon_teardown: false when game_state below threshold.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 0u;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, true, true);
        ok &= expect_false(d.ready_for_abandon_teardown, "not ready when game_state below threshold");
    }

    // arcade_launch_failed_before_connect: true for a stuck custom-game launch (RunQueued, not yet Loaded,
    // owner not connected, direct path). This branch suppresses the normal teardown + postgame sequence.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        lobby.owner_connected = false;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_true(d.arcade_launch_failed_before_connect, "arcade launch failed before connect");
        ok &= expect_false(d.treat_as_current_game_disconnect, "arcade failed not treated as current-game disconnect");
        ok &= expect_true(d.queue_cache_unsubscribed, "arcade failed queues cache unsubscribed");
        ok &= expect_true(d.set_pending_reset_after_cache_unsubscribed, "arcade failed marks pending reset");
        ok &= expect_true(d.discard_queued_launch_messages, "arcade failed discards launch queue");
        ok &= expect_true(d.suppress_abandoned_lobby, "arcade failed suppresses abandoned lobby");
        ok &= expect_false(d.queue_postgame_teardown, "arcade failed skips postgame teardown");
    }

    // arcade_launch_failed_before_connect: false once launch reaches Loaded (launch completed).
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseLoaded;  // >= Loaded
        lobby.owner_connected = false;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_false(d.arcade_launch_failed_before_connect, "arcade failed false when loaded");
        ok &= expect_true(d.ready_for_abandon_teardown, "loaded arcade lobby ready for teardown");
    }

    // arcade_launch_failed_before_connect: false before RunQueued (launch not yet queued).
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;  // < RunQueued
        lobby.owner_connected = false;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_false(d.arcade_launch_failed_before_connect, "arcade failed false before run queued");
    }

    // arcade_launch_failed_before_connect: false on wrapped path (wrapped abandon is user-initiated leave).
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        lobby.owner_connected = false;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, true, true);  // wrapped
        ok &= expect_false(d.arcade_launch_failed_before_connect, "arcade failed false on wrapped path");
    }

    // arcade_launch_failed_before_connect: false when owner connected (launch is healthy).
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        lobby.owner_connected = true;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_false(d.arcade_launch_failed_before_connect, "arcade failed false when owner connected");
    }

    // arcade_launch_failed_before_connect: false for non-custom lobby.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        lobby.owner_connected = false;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_false(d.arcade_launch_failed_before_connect, "arcade failed false for non-custom lobby");
    }

    // arcade_launch_failed_before_connect: false when game_state < 2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.custom_game.game_id = 500ull;
        lobby.state = 2u;
        lobby.game_state = 1u;
        lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
        lobby.owner_connected = false;
        auto d = gbe::dota_lobby_state::compute_abandon_decision(lobby, false, true);
        ok &= expect_false(d.arcade_launch_failed_before_connect, "arcade failed false when game_state below 2");
    }

    return ok;
}

bool test_teardown_retrieval_decision()
{
    bool ok = true;

    {
        auto d = gbe::dota_lobby_state::compute_teardown_retrieval_decision(
            true,
            true,
            false,
            false,
            GBE_kDotaOtherLeftChannel,
            true);
        ok &= expect_true(d.finalize_abandon_after_7014, "7014 matching channel finalizes abandon");
        ok &= expect_false(d.finalize_normal_signout_after_25, "7014 does not finalize normal signout");
        ok &= expect_false(d.reset_after_cache_unsubscribed, "7014 does not reset after 25");
    }

    {
        auto d = gbe::dota_lobby_state::compute_teardown_retrieval_decision(
            true,
            true,
            false,
            false,
            GBE_kDotaOtherLeftChannel,
            false);
        ok &= expect_false(d.finalize_abandon_after_7014, "7014 with stale channel does not finalize abandon");
    }

    {
        auto d = gbe::dota_lobby_state::compute_teardown_retrieval_decision(
            true,
            false,
            true,
            true,
            GBE_kDotaCacheUnsubscribed,
            false);
        ok &= expect_false(d.finalize_abandon_after_7014, "25 does not finalize abandon");
        ok &= expect_true(d.finalize_normal_signout_after_25, "25 finalizes normal signout");
        ok &= expect_true(d.reset_after_cache_unsubscribed, "25 applies pending reset");
    }

    {
        auto d = gbe::dota_lobby_state::compute_teardown_retrieval_decision(
            false,
            true,
            true,
            true,
            GBE_kDotaCacheUnsubscribed,
            true);
        ok &= expect_false(d.finalize_abandon_after_7014, "non-Dota profile skips abandon finalize");
        ok &= expect_false(d.finalize_normal_signout_after_25, "non-Dota profile skips normal finalize");
        ok &= expect_false(d.reset_after_cache_unsubscribed, "non-Dota profile skips pending reset");
    }

    return ok;
}

bool test_postgame_observation_decision()
{
    bool ok = true;

    {
        auto d = gbe::dota_lobby_state::compute_postgame_observation_decision(
            false,
            true,
            true,
            2u,
            3u,
            100ull);
        ok &= expect_true(d.skip_for_host_client, "host client postgame observation skips player cleanup");
        ok &= expect_false(d.skip_for_arcade_active_match, "host client skip takes precedence over arcade skip");
        ok &= expect_false(d.run_player_cleanup, "host client must preserve server-owned shared state");
    }

    {
        auto d = gbe::dota_lobby_state::compute_postgame_observation_decision(
            false,
            false,
            false,
            2u,
            3u,
            100ull);
        ok &= expect_false(d.skip_for_host_client, "player cleanup path has no host server GC");
        ok &= expect_false(d.skip_for_arcade_active_match, "player cleanup path is not active arcade");
        ok &= expect_true(d.run_player_cleanup, "non-host client postgame transition runs player cleanup");
    }

    {
        auto d = gbe::dota_lobby_state::compute_postgame_observation_decision(
            false,
            false,
            true,
            2u,
            3u,
            100ull);
        ok &= expect_false(d.skip_for_host_client, "arcade active skip has no host server GC");
        ok &= expect_true(d.skip_for_arcade_active_match, "arcade active match skips player cleanup");
        ok &= expect_false(d.run_player_cleanup, "arcade active match preserves runtime state");
    }

    {
        auto d = gbe::dota_lobby_state::compute_postgame_observation_decision(
            true,
            false,
            false,
            2u,
            3u,
            100ull);
        ok &= expect_false(d.skip_for_host_client, "server GC does not use client postgame observation skip");
        ok &= expect_false(d.skip_for_arcade_active_match, "server GC does not use arcade client skip");
        ok &= expect_false(d.run_player_cleanup, "server GC does not run player observation cleanup");
    }

    {
        auto d = gbe::dota_lobby_state::compute_postgame_observation_decision(
            false,
            false,
            false,
            3u,
            3u,
            100ull);
        ok &= expect_false(d.skip_for_host_client, "already-postgame state is not a transition");
        ok &= expect_false(d.run_player_cleanup, "already-postgame state does not run cleanup again");
    }

    {
        auto d = gbe::dota_lobby_state::compute_postgame_observation_decision(
            false,
            false,
            false,
            2u,
            3u,
            0ull);
        ok &= expect_false(d.run_player_cleanup, "zero lobby id blocks player cleanup side effects");
    }

    return ok;
}

bool test_active_lobby_owned_by_local_user()
{
    bool ok = true;

    GBE_LocalLobby lobby = make_active_lobby();
    lobby.owner_steam_id = 0x100000u;
    ok &= expect_true(
        gbe::dota_lobby_state::is_active_lobby_owned_by_local_user(lobby, 100ull, 0x100000u),
        "active matching lobby owned by local user");

    ok &= expect_false(
        gbe::dota_lobby_state::is_active_lobby_owned_by_local_user(lobby, 100ull, 0ull),
        "zero local steam id is not owner");
    ok &= expect_false(
        gbe::dota_lobby_state::is_active_lobby_owned_by_local_user(lobby, 100ull, 0x200000u),
        "remote owner is not local owner");
    ok &= expect_false(
        gbe::dota_lobby_state::is_active_lobby_owned_by_local_user(lobby, 101ull, 0x100000u),
        "different lobby id is not active owned lobby");

    lobby.active = false;
    ok &= expect_false(
        gbe::dota_lobby_state::is_active_lobby_owned_by_local_user(lobby, 100ull, 0x100000u),
        "inactive lobby is not active owned lobby");

    return ok;
}

bool test_serialized_connection_state_scopes_dedup_to_generation()
{
    bool ok = true;
    GBE_DotaSerializedConnectionState state{};

    state.begin_generation(1ull);
    ok &= expect_true(state.generation == 1ull, "serialized state records lobby generation");
    ok &= expect_true(state.should_connect_direct(700ull, "10.0.0.5:27015"), "first direct connect is allowed");
    state.record_direct_connect(700ull, "10.0.0.5:27015");
    ok &= expect_false(state.should_connect_direct(700ull, "10.0.0.5:27015"), "same lobby direct connect is deduplicated");
    ok &= expect_true(state.should_connect_direct(700ull, "10.0.0.6:27015"), "same lobby allows a different direct endpoint");
    state.record_direct_connect(700ull, "10.0.0.6:27015");
    ok &= expect_false(state.should_connect_direct(700ull, "10.0.0.6:27015"), "changed direct endpoint becomes the new deduplication key");

    state.record_engine_callback(700ull, "10.0.0.5:27015");
    ok &= expect_true(state.engine_callback_queued(700ull, "10.0.0.5:27015"), "same lobby engine callback is deduplicated");
    ok &= expect_false(state.engine_callback_queued(700ull, "10.0.0.6:27015"), "same lobby allows a callback for a different endpoint");
    state.record_engine_callback(700ull, "10.0.0.6:27015");
    ok &= expect_true(state.engine_callback_queued(700ull, "10.0.0.6:27015"), "changed callback endpoint becomes the new deduplication key");

    GBE_DotaSerializedConnectionState other_instance{};
    other_instance.begin_generation(1ull);
    ok &= expect_true(other_instance.should_connect_direct(700ull, "10.0.0.5:27015"), "serialized socket instances keep independent direct connect state");
    ok &= expect_false(other_instance.engine_callback_queued(700ull, "10.0.0.5:27015"), "serialized socket instances keep independent callback state");

    state.retry_count = 4u;
    state.last_post_size = 512u;
    state.begin_server(701ull);
    ok &= expect_true(state.should_connect_direct(701ull, "10.0.0.5:27015"), "same lobby allows a different direct server");
    ok &= expect_false(state.engine_callback_queued(701ull, "10.0.0.5:27015"), "same lobby allows a callback for a different server");
    ok &= expect_true(state.retry_count == 0u, "server change resets retry accounting");
    ok &= expect_true(state.last_post_size == 0u, "server change resets the last posted payload size");
    ok &= expect_true(state.callback_key == gbe::dota_connection::DedupKey{}, "server change clears callback deduplication state");

    state.begin_generation(1ull);
    ok &= expect_false(state.should_connect_direct(700ull, "10.0.0.6:27015"), "same generation preserves direct-connect deduplication");

    state.retry_count = 4u;
    state.last_post_size = 512u;
    state.begin_generation(9ull);
    ok &= expect_true(state.generation == 9ull, "serialized state stores the current generation");
    ok &= expect_true(state.last_post_server_id == 0ull, "generation change resets the posted server generation");
    ok &= expect_true(state.retry_count == 0u, "generation change resets retry accounting");
    ok &= expect_true(state.last_post_size == 0u, "generation change clears the last posted payload size");
    ok &= expect_true(state.callback_key == gbe::dota_connection::DedupKey{}, "generation change clears callback deduplication state");
    ok &= expect_true(state.direct_connect_key == gbe::dota_connection::DedupKey{}, "generation change clears direct connect deduplication state");
    state.record_direct_connect(700ull, "10.0.0.5:27015");
    state.record_engine_callback(700ull, "10.0.0.5:27015");
    state.begin_generation(10ull);
    ok &= expect_true(state.generation == 10ull, "serialized state advances to the next generation");
    ok &= expect_true(state.should_connect_direct(700ull, "10.0.0.5:27015"), "new generation allows the same direct endpoint");
    ok &= expect_false(state.engine_callback_queued(700ull, "10.0.0.5:27015"), "new generation allows the same engine callback endpoint");
    return ok;
}

bool test_generation_change_clears_connection_dedup_properties()
{
    bool ok = true;
    for (std::uint64_t seed = 1u; seed <= 64u; ++seed) {
        const std::uint64_t server_id = 2000u + seed;
        const std::string endpoint = "10.20.30." + std::to_string(seed) + ":27015";
        GBE_DotaSerializedConnectionState state{};
        state.begin_generation(seed);
        state.begin_server(server_id);
        state.retry_count = static_cast<std::uint32_t>(seed);
        state.last_post_size = static_cast<std::uint32_t>(seed * 17u);
        state.record_direct_connect(server_id, endpoint);
        state.record_engine_callback(server_id, endpoint);

        state.begin_generation(seed + 1u);

        ok &= expect_eq_u64(state.generation, seed + 1u, "P6-C generation change stores the new generation");
        ok &= expect_eq_u64(state.last_post_server_id, 0u, "P6-C generation change clears posted server state");
        ok &= expect_eq_u32(state.retry_count, 0u, "P6-C generation change clears retry state");
        ok &= expect_eq_u32(state.last_post_size, 0u, "P6-C generation change clears payload size state");
        ok &= expect_true(state.callback_key == gbe::dota_connection::DedupKey{}, "P6-C generation change clears callback dedup state");
        ok &= expect_true(state.direct_connect_key == gbe::dota_connection::DedupKey{}, "P6-C generation change clears direct-connect dedup state");
        ok &= expect_true(state.should_connect_direct(server_id, endpoint), "P6-C new generation restores direct-connect opportunity");
        ok &= expect_false(state.engine_callback_queued(server_id, endpoint), "P6-C new generation restores callback opportunity");
    }
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_local_lobby_owner_boundaries();
    ok &= test_valid_launch_progression();
    ok &= test_launch_lifecycle_transition_decision();
    ok &= test_launch_lifecycle_action_sequence();
    ok &= test_lifecycle_action_properties();
    ok &= test_lobby_generation_allocation_rules();
    ok &= test_lobby_generation_properties();
    ok &= test_stale_generic_lobby_state_regression();
    ok &= test_owner_disconnect_and_reconnect();
    ok &= test_reconnect_eligibility_decision();
    ok &= test_reconnect_context_source_pipeline();
    ok &= test_reconnect_interception_decision();
    ok &= test_runtime_reset_reconnect_preserve_decision();
    ok &= test_post_game_teardown_suppression();
    ok &= test_teardown_retrieval_decision();
    ok &= test_postgame_observation_decision();
    ok &= test_active_lobby_owned_by_local_user();
    ok &= test_serialized_connection_state_scopes_dedup_to_generation();
    ok &= test_generation_change_clears_connection_dedup_properties();

    if (!ok)
        return 1;

    std::cout << "gbe_dota_lobby_state_test passed" << std::endl;
    return 0;
}
