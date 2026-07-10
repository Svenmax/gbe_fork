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
#include "dll/dll/gbe_dota_serialized_connection_state.h"
#include "dll/gbe_dota_protocol_constants.h"

#include <cstring>
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
        bool result = gbe::dota_lobby_state::build_reconnect_context(lobby, ctx);
        ok &= expect_true(result, "reconnect context built for started game");
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
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision("7035_disconnect_current_game_after_25");
        ok &= expect_true(d.preserve_reconnect_context, "7035 current-game disconnect reset preserves reconnect context");
    }

    {
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision("shutdown_gc");
        ok &= expect_false(d.preserve_reconnect_context, "shutdown reset clears reconnect context");
    }

    {
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision("7040_leave_practice_lobby");
        ok &= expect_false(d.preserve_reconnect_context, "ordinary leave reset clears reconnect context");
    }

    {
        auto d = gbe::dota_lobby_state::compute_runtime_reset_decision(nullptr);
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

bool test_serialized_connection_state_scopes_dedup_to_lobby()
{
    bool ok = true;
    GBE_DotaSerializedConnectionState state{};

    state.begin_lobby(100ull);
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
    other_instance.begin_lobby(100ull);
    ok &= expect_true(other_instance.should_connect_direct(700ull, "10.0.0.5:27015"), "serialized socket instances keep independent direct connect state");
    ok &= expect_false(other_instance.engine_callback_queued(700ull, "10.0.0.5:27015"), "serialized socket instances keep independent callback state");

    state.retry_count = 4u;
    state.last_post_size = 512u;
    state.begin_server(701ull);
    ok &= expect_true(state.should_connect_direct(701ull, "10.0.0.5:27015"), "same lobby allows a different direct server");
    ok &= expect_false(state.engine_callback_queued(701ull, "10.0.0.5:27015"), "same lobby allows a callback for a different server");
    ok &= expect_true(state.retry_count == 0u, "server change resets retry accounting");
    ok &= expect_true(state.last_post_size == 0u, "server change resets the last posted payload size");
    ok &= expect_true(state.callback_server_id == 0ull && state.callback_endpoint.empty(), "server change clears callback deduplication state");

    state.begin_lobby(101ull);
    ok &= expect_true(state.should_connect_direct(700ull, "10.0.0.5:27015"), "new lobby allows the same direct endpoint");
    ok &= expect_false(state.engine_callback_queued(700ull, "10.0.0.5:27015"), "new lobby allows the same engine callback endpoint");
    ok &= expect_true(state.last_post_server_id == 0ull, "new lobby resets the posted server generation");
    ok &= expect_true(state.retry_count == 0u, "new lobby resets retry accounting");
    ok &= expect_true(state.callback_server_id == 0ull && state.callback_endpoint.empty(), "new lobby clears callback deduplication state");
    ok &= expect_true(state.direct_connect_server_id == 0ull && state.direct_connect_endpoint.empty(), "new lobby clears direct connect deduplication state");
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_valid_launch_progression();
    ok &= test_launch_lifecycle_transition_decision();
    ok &= test_stale_generic_lobby_state_regression();
    ok &= test_owner_disconnect_and_reconnect();
    ok &= test_reconnect_eligibility_decision();
    ok &= test_reconnect_interception_decision();
    ok &= test_runtime_reset_reconnect_preserve_decision();
    ok &= test_post_game_teardown_suppression();
    ok &= test_teardown_retrieval_decision();
    ok &= test_postgame_observation_decision();
    ok &= test_active_lobby_owned_by_local_user();
    ok &= test_serialized_connection_state_scopes_dedup_to_lobby();

    if (!ok)
        return 1;

    std::cout << "gbe_dota_lobby_state_test passed" << std::endl;
    return 0;
}
