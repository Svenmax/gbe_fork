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
#include "dll/gbe_dota_types.h"
#include "dll/dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_protocol_constants.h"

#include <iostream>
#include <string>

namespace {

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;
    std::cerr << "failed: " << label << std::endl;
    return false;
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
        shared.custom_game.game_id = 500ull;
        shared.state = 4u;  // READYUP
        shared.game_state = 2u;  // in-game
        GBE_LocalLobby local{};
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, true, local);
        ok &= expect_eq_u32(local.state, 2u, "adopt normalizes stale readyup to run for custom game");
        ok &= expect_eq_u32(local.game_state, 2u, "adopt keeps game_state");
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

    // compose_queued_lobby_state_apply_plan: preserve_monotonic_game_state prevents stale reset.
    {
        GBE_LocalLobby lobby = make_active_lobby();
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
        bool result = gbe::dota_lobby_state::build_reconnect_context(lobby, ctx);
        ok &= expect_true(result, "reconnect context built for started game");
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
        ok &= expect_false(gbe::dota_lobby_state::build_reconnect_context(lobby, ctx), "reconnect rejected when inactive");
    }

    // build_reconnect_context: rejected when game not started (state<2 AND game_state<2).
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;
        lobby.game_state = 1u;  // both below 2
        lobby.server_id = 700ull;
        lobby.connect = "1.2.3.4:27015";
        GBE_DotaReconnectContext ctx{};
        ok &= expect_false(gbe::dota_lobby_state::build_reconnect_context(lobby, ctx), "reconnect rejected when game not started");
    }

    // build_reconnect_context: accepted when game_state>=2 even if state<2.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 1u;
        lobby.game_state = 2u;  // game_state carries the start condition
        lobby.server_id = 700ull;
        lobby.connect = "1.2.3.4:27015";
        GBE_DotaReconnectContext ctx{};
        ok &= expect_true(gbe::dota_lobby_state::build_reconnect_context(lobby, ctx), "reconnect accepted via game_state>=2");
    }

    // build_reconnect_context: rejected when server_id is zero.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.server_id = 0ull;
        lobby.connect = "1.2.3.4:27015";
        GBE_DotaReconnectContext ctx{};
        ok &= expect_false(gbe::dota_lobby_state::build_reconnect_context(lobby, ctx), "reconnect rejected when server_id zero");
    }

    // build_reconnect_context: rejected when connect empty.
    {
        GBE_LocalLobby lobby = make_active_lobby();
        lobby.state = 2u;
        lobby.game_state = 2u;
        lobby.server_id = 700ull;
        lobby.connect = "";
        GBE_DotaReconnectContext ctx{};
        ok &= expect_false(gbe::dota_lobby_state::build_reconnect_context(lobby, ctx), "reconnect rejected when connect empty");
    }

    return ok;
}

// ---- Category 4: post-game teardown suppression ---------------------------

bool test_post_game_teardown_suppression()
{
    bool ok = true;

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

} // namespace

int main()
{
    bool ok = true;
    ok &= test_valid_launch_progression();
    ok &= test_stale_generic_lobby_state_regression();
    ok &= test_owner_disconnect_and_reconnect();
    ok &= test_post_game_teardown_suppression();

    if (!ok)
        return 1;

    std::cout << "gbe_dota_lobby_state_test passed" << std::endl;
    return 0;
}
