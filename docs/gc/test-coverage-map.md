# Dota GC Test Coverage Map

This document maps the offline GC tests to the behavior they protect. Use it to decide which test to extend before changing lifecycle, shared-state, payload, or side-effect behavior.

## Entry Points

| Command | Use |
| --- | --- |
| `bash tools/run_gc_offline_tests.sh` | Fast high-signal offline compile/run suite. |
| `bash tools/run_gc_offline_tests.sh --full` | Complete offline suite, including extra replay fixtures and focused lobby/custom-game tests. |
| `bash tools/run_gc_verification.sh --fast` | PR-friendly gate: fast offline tests plus refactor audit and `git diff --check`. |
| `bash tools/run_gc_verification.sh --full` | Handoff gate: full offline tests plus refactor audit and `git diff --check`. |

The PR workflow runs `bash tools/run_gc_verification.sh --fast`. Local handoff for GC refactors should run the full gate.

## Fast Offline Suite

| Binary / Fixture | Main Coverage | Good For | Limits |
| --- | --- | --- | --- |
| `gc_message_utils_test` | GC message wrapper helpers. | Wrapped payload/session/source-job utility changes. | Does not cover Dota lifecycle semantics. |
| `gbe_gc_config_test` | GC configuration parsing and defaults. | Config-only changes. | No handler or payload coverage. |
| `gbe_proto_wire_test` | Wire parsing/building, router/lobby helper integration. | Protobuf-like encoding/decoding changes. | Does not assert live coordinator side effects. |
| `gc_replay_test minimal` | Minimal replay parsing and expected summary output. | Replay harness sanity. | Very broad, low lifecycle specificity. |
| `gc_replay_test practice_lobby` | Practice lobby replay behavior. | Lobby creation/publication regressions. | Fixture-level signal only; not precise about internal state. |
| `gc_replay_test game_flow` | Game flow replay behavior. | Launch/game-state flow regressions. | Does not isolate individual helpers. |
| `gc_replay_test cache_and_items` | Cache and item replay behavior. | Inventory/cache response regressions. | Limited lifecycle cleanup assertions. |
| `gbe_dota_gc_payload_helpers_test` | Payload helper behavior, reconnect decisions, launch/replay payload helpers. | Read-only shared-state facade and payload transformation changes. | Uses test wrapper/stubs, not the full production coordinator. |
| `gbe_dota_handler_test` | Handler smoke tests, side-effect recorder, inventory/chat/lobby flow ordering. | Handler ordering, reset helper contracts, publish reasons. | Some coordinator behavior is represented by stubs; production helper equivalence should stay small and obvious. |

## Full-Only Additions

| Binary / Fixture | Main Coverage | Good For | Limits |
| --- | --- | --- | --- |
| `gc_replay_test chat_channel` | Chat-channel replay behavior. | Chat join/leave/broadcast channel regressions. | Fixture output may not identify exact internal state cause. |
| `gc_replay_test lobby_lifecycle` | Lobby lifecycle replay behavior. | Signout, cleanup, and lifecycle ordering regressions. | Should be paired with focused tests for new lifecycle branches. |
| `gc_replay_test wire_edge_cases` | Wire edge-case replay behavior. | Parser edge cases and wrapper metadata. | No direct lifecycle intent coverage. |
| `gbe_dota_lobby_flow_test` | Pure lobby flow helpers, publish composition, and launch-state planner/payload/action seams. | Lifecycle planning helpers, flow transitions, and launch-state push seam changes. | Does not execute full handler side effects or link the full launch coordinator TU. |
| `gbe_dota_lobby_state_test` | Lobby state decision helpers. | Reconnect eligibility/state transition changes. | Pure/helper oriented. |
| `gbe_dota_custom_game_test` | Custom-game metadata, publish data, HTTP parsing helpers. | Arcade/custom-game flow and item data changes. | Does not cover full coordinator side effects. |

## Handler Smoke Tests With High Maintenance Value

These tests are especially relevant to future GC refactor work:

| Test | Protects |
| --- | --- |
| `test_chat_join_channel` | Chat join local mutation, publish behavior, and response shape. |
| `test_chat_leave_postgame_channel_order` | Postgame leave-chat response ordering and publish reason. |
| `test_lobby_abandon_current_game_disconnect_queues_25` | Abandon disconnect queues cache unsubscribe. |
| `test_lobby_leave_queues_25_then_clears_local_lobby` | Leave path queues cache unsubscribe before local cleanup. |
| `test_lobby_destroy_queues_25_then_8247_and_clears_lobby` | Destroy path side-effect ordering and cleanup. |
| `test_lobby_kick_removes_member_then_publishes_details` | Member mutation before details publication. |
| `test_lobby_set_details_mutates_before_publish_and_details_update` | Set-details mutation/publish/details update order. |
| `test_lobby_abandon_ready_teardown_queues_postgame_response` | Abandon-ready teardown postgame response behavior. |
| `test_lobby_normal_signout_pending_clear_resets_state` | Normal signout pending cleanup reset behavior. |
| `test_lobby_runtime_reset_clears_local_shared_and_last_launch_state` | Runtime reset contract for local/shared/last-launch state. |
| `test_lobby_custom_launch_updates_rich_presence_before_setup_flow` | Custom-game 7041 launch records shared publish, rich-presence update, and launch persona before setup-flow handling. |
| `test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state` | Host-client postgame observation preserves server-owned shared state while still observing the transition. |
| `test_lobby_host_client_postgame_observation_ignores_mismatched_server_lobby` | Mismatched server-GC lobby id runs player cleanup instead of host-client preserve. |
| `test_lobby_player_postgame_observation_clears_shared_state_after_details_update` | Ordinary player postgame observation pushes details update before cache unsubscribe and clears local/shared state. |
| `test_lobby_arcade_active_postgame_observation_preserves_shared_state` | Arcade active postgame observation preserves local/shared state and skips cleanup messages. |
| `test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip` | Host-client ownership preserves server-owned state when both predicates are true, with active arcade runtime details-update suppression preserved. |

Inventory tests in the same binary protect item unlock/equip behavior and should be used for inventory/VPK/template work, but they are less central to lobby lifecycle refactors.

## What To Extend For Common Changes

| Change Type | First Test To Add/Update | Also Run |
| --- | --- | --- |
| Direct or facade read of `GBE_shared_dota_lobby_state` | `gbe_dota_gc_payload_helpers_test` or handler smoke test that compares snapshot behavior. | Full verification. |
| Shared-state clear helper | Handler smoke test proving behavior-equivalent clear. | `lobby_lifecycle` replay fixture. |
| Full runtime reset helper | `gbe_dota_handler_test` reset contract test. | Full verification. |
| Stale postgame chat leave | `test_chat_leave_postgame_channel_order` or a new adjacent handler smoke test. | `chat_channel` replay fixture. |
| Normal signout/cache unsubscribe | Existing normal-signout test or a new focused handler/lifecycle test. | `lobby_lifecycle` replay fixture. |
| Reconnect eligibility or payload | `gbe_dota_gc_payload_helpers_test`. | `game_flow` and `lobby_lifecycle` fixtures. |
| Message ordering around `push_incoming_*` | Handler smoke test using the side-effect recorder. | Relevant replay fixture. |
| Pure lobby flow/state helper | `gbe_dota_lobby_flow_test` or `gbe_dota_lobby_state_test`. | Full verification. |
| Custom-game metadata or joinable lobby data | `gbe_dota_custom_game_test`. | `practice_lobby` replay fixture if visible in output. |
| Wire format or wrapper metadata | `gbe_proto_wire_test` or `gc_message_utils_test`. | Replay fixtures that include the message shape. |

## Known Coverage Gaps

- The handler reset test currently protects the handler-test stub contract for `GBE_ClearDotaLobbyRuntimeState()`. The production implementation is intentionally small and branch-free; add production-linked coverage only if it gains branching or new side effects.
- Postgame observation is covered by `compute_postgame_observation_decision(...)` focused tests plus handler-level host-client preserve, ordinary player cleanup, arcade-active preserve, and host-over-arcade precedence tests. The server-GC active-lobby owner predicate is covered by `is_active_lobby_owned_by_local_user(...)` focused tests. Add more handler coverage before changing cleanup side-effect ordering.
- Reconnect preserve-vs-clear behavior is covered by `compute_runtime_reset_decision(...)` focused tests; extend coverage if more reset reasons preserve context.
- `GBE_IsSharedDotaArcadeLobbyActive()` and `GBE_GetSharedDotaReconnectStateSnapshot()` are covered against direct-field behavior; keep broad shared-state read replacement to one tested read-only cluster at a time.
- Side-effect recorder coverage should be extended before wrapping more push/broadcast/detail-update operations.
- Launch-state push planner, payload build request, grouped payload build, target/shared/capture/captured-context mapping, and action-sequence seams are covered by `gbe_dota_lobby_flow_test`; full launch coordinator harness coverage remains deferred around restore, capture source, settings read source, logging, and coordinator selection.

## Maintenance Rule

When adding a new production GC `.cpp` file, update every relevant explicit source list:

- `tools/run_gc_offline_tests.sh`
- `premake5.lua` optional GC test targets, when applicable

Then run:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

If the new test only passes because of a stub, say so in the review notes. Stub coverage is useful, but it should not be mistaken for production linkage coverage.
