# Dota GC Lifecycle State Map

This document maps the main Dota lobby lifecycle paths to the state and side effects they touch. It is meant to help future bug investigations and small refactors choose the right tests before changing behavior.

## State Names

| State | Meaning |
| --- | --- |
| `GBE_local_lobby` | Local coordinator working state for the current practice/custom lobby flow. |
| `GBE_shared_dota_lobby_state` | Shared snapshot consumed across handler, payload, server/client, reconnect, and postgame paths. |
| Last pushed launch game state | Deduplication state for launch/rich-presence style updates. |
| Pending normal signout state | Temporary lifecycle flag/context used to finish cleanup after cache unsubscribe. |
| Reconnect context | Context used to build reconnect-related payloads and decide reconnect eligibility. |

## Main Paths

### Lobby Creation And Publication

Typical files:

- `dll/gbe_dota_lobby_handlers.cpp`
- `dll/gbe_dota_lobby_state_coordinator.cpp`
- `dll/gbe_dota_lobby_snapshot_coordinator.cpp`
- `dll/gbe_dota_payload_lobby_helpers.cpp`

Expected behavior:

- Mutate `GBE_local_lobby` to represent the created or joined lobby.
- Publish a shared snapshot through `GBE_PublishSharedDotaLobbyState(reason)`.
- Queue details/cache responses in the existing order.
- Preserve request/session/source-job wrapping where the handler already does so.

Risk:

- Publishing before all local fields are updated can expose partial lobby state.
- Publishing after a cleanup path can resurrect stale local state.

Useful tests:

- Handler smoke tests around lobby create/leave/destroy/set-details ordering.
- Replay fixtures for practice lobby and lobby lifecycle.
- Lobby state focused tests for pure transition logic.

### Chat Join And Broadcast Channel Updates

Typical file:

- `dll/gbe_dota_chat_handlers.cpp`

Expected behavior:

- Chat join mutates local chat fields and publishes shared lobby state.
- Broadcast join/update/close mutates broadcast fields, publishes, and sends details update.
- Leave-chat has separate normal, postgame, legacy, and stale-channel branches.

Risk:

- Leave-chat ordering is message-sensitive.
- The stale postgame path must not re-publish shared state after normal signout cleared it.

Useful tests:

- `test_chat_join_channel`
- `test_chat_leave_postgame_channel_order`
- `chat_channel` replay fixture in the full offline suite.

### Launch And In-Game Progression

Typical files:

- `dll/gbe_dota_lobby_launch_coordinator.cpp`
- `dll/gbe_dota_lobby_flow_coordinator.cpp`
- `dll/gbe_dota_gc_payload_helpers.cpp`

Expected behavior:

- Track launch/game-state progression.
- Push launch/rich-presence related updates without duplicate game-state spam.
- Preserve reconnect-relevant fields when a path is not a full runtime reset.

Risk:

- Clearing shared state can change reconnect eligibility.
- Clearing last pushed launch state too early can change observable update ordering.

Useful tests:

- Payload helper reconnect tests.
- Lobby lifecycle replay fixture.
- Future focused tests should assert last-launch clearing separately from shared-state clearing.

### Abandon And Postgame Cleanup

Typical files:

- `dll/gbe_dota_lobby_flow_coordinator.cpp`
- `dll/gbe_dota_chat_handlers.cpp`
- `dll/gbe_dota_lobby_handlers.cpp`

Expected behavior:

- Queue cache unsubscribe or postgame responses in existing order.
- Distinguish player postgame cleanup from full runtime reset.
- Preserve server-owned state when a host client is only observing postgame.

Risk:

- A broad reset can clear state still needed by server GC.
- A narrow local-only cleanup can leave stale shared state if used on the wrong path.

Useful tests:

- `test_lobby_abandon_current_game_disconnect_queues_25`
- `test_lobby_abandon_ready_teardown_queues_postgame_response`
- `test_chat_leave_postgame_channel_order`
- `test_chat_leave_postgame_skips_stale_republish_after_shared_clear`
- `test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state`
- `test_lobby_player_postgame_observation_clears_shared_state_after_details_update`
- `test_lobby_arcade_active_postgame_observation_preserves_shared_state`
- `test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip`
- Focused postgame observation decision tests in `gbe_dota_lobby_state_test`.

### Normal Signout And Cache Unsubscribe

Typical files:

- `dll/gbe_dota_lobby_flow_coordinator.cpp`
- `dll/gbe_dota_lobby_state_coordinator.cpp`
- `dll/steam_game_coordinator.cpp`

Expected behavior:

- Mark or preserve pending normal-signout cleanup until the cache-unsubscribe boundary.
- Finalize cleanup after cache unsubscribe.
- Clear local lobby, shared lobby state, and last pushed launch state through `GBE_ClearDotaLobbyRuntimeState()` where a full runtime reset is intended.

Risk:

- Clearing before cache unsubscribe can change message ordering.
- Failing to clear after finalize can leave stale lobby state.
- Re-publishing local state after finalize can undo the cleanup.

Useful tests:

- `test_lobby_normal_signout_pending_clear_resets_state`
- `test_lobby_runtime_reset_clears_local_shared_and_last_launch_state`
- Lobby lifecycle replay fixture.

### Reconnect

Typical files:

- `dll/gbe_dota_gc_payload_helpers.cpp`
- `dll/gbe_dota_reconnect_shared.h`
- `dll/gbe_dota_lobby_flow_coordinator.cpp`

Expected behavior:

- Compute reconnect eligibility from shared state, local context, game state, server id, connect string, and custom-game fields.
- Preserve reconnect context on paths that are not full runtime reset.

Risk:

- A facade that hides fields without naming reconnect intent can accidentally clear or default required metadata.
- Tests that only check handler success may miss changed reconnect payload content.

Useful tests:

- Payload helper reconnect tests in `gbe_dota_gc_payload_helpers_test`.
- Focused runtime reset decision tests in `gbe_dota_lobby_state_test` assert preserve vs clear behavior.

## Change Rules

When changing a lifecycle path, first classify the path:

| Class | Examples | Required care |
| --- | --- | --- |
| Read-only decision | reconnect eligibility, lobby id lookup | Prefer snapshot/accessor helpers. |
| Publish | local lobby to shared state | Preserve local mutation before publish and response order after publish. |
| Full runtime reset | normal signout finalize, explicit runtime reset | Use `GBE_ClearDotaLobbyRuntimeState()`. |
| Partial cleanup | stale chat leave, host-client observation | Keep the exception explicit and covered by tests. |
| Side-effect sequence | cache unsubscribe, postgame response, details update | Assert order in handler smoke tests or replay fixtures. |

## Focused Coverage Status

- Host client observes postgame and preserves shared state needed by server GC: covered by `test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state`.
- Mismatched server-GC lobby id runs player cleanup instead of host-client preserve: covered by `test_lobby_host_client_postgame_observation_ignores_mismatched_server_lobby`.
- Host-client ownership takes precedence over arcade-active cleanup skip behavior: covered by `test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip`.
- Server-GC active-lobby ownership predicate is covered by `test_active_lobby_owned_by_local_user`.
- Ordinary player postgame cleanup clears shared state after details update: covered by `test_lobby_player_postgame_observation_clears_shared_state_after_details_update`.
- Arcade-active postgame observation preserves shared state: covered by `test_lobby_arcade_active_postgame_observation_preserves_shared_state`.
- Reconnect context survives preserve paths and clears on ordinary reset paths: covered by focused lobby-state tests and handler reset coverage.
- Shared-state clear helper remains behavior-equivalent for runtime reset: covered by `test_lobby_runtime_reset_clears_local_shared_and_last_launch_state`.
- Read-only shared-state facade raw snapshot and valid-gated helper semantics: covered by payload helper tests.
- Postgame stale chat leave cannot re-publish a cleared shared state: covered by `test_chat_leave_postgame_skips_stale_republish_after_shared_clear`.
- Custom-game 7041 launch updates rich presence and launch persona state before setup-flow handling: covered by `test_lobby_custom_launch_updates_rich_presence_before_setup_flow`.
- Launch-state push planner, target/captured input mapping, payload build request sequencing, grouped payload building, and action-sequence execution are covered by focused flow tests plus production helper integration.

## Missing Tests Worth Adding Next

- Full launch-state client-peer push harness remains deferred until the remaining restore/capture/settings/logging boundary can be represented by a small explicit context.
- Production-linked reset test re-check remains conditional on `GBE_ClearDotaLobbyRuntimeState()` gaining branching, extra side effects, or semantic reset ownership.

## Lifecycle SM vs Legacy effects (C2)

The pure state machine in `gbe_dota_lifecycle_state_machine.h` is only partially wired. Named gate effects (`RuntimeMemberUpdateRequested`, `RuntimeGameStateUpdateRequested`, `PracticeLobbyDetailsRequested`, `TeardownActionsRequested`, `CustomGameLifecycleActionsRequested`) already gate production paths. Custom-game still runs imperative `compute_*` + Execute after the named gate.

Authoritative call-site tables and migration order: [LEGACY_LIFECYCLE_EFFECTS.md](./LEGACY_LIFECYCLE_EFFECTS.md).

## Verification

Before merging lifecycle changes, run:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

For risky lifecycle changes, add or update the focused offline test before changing production behavior.
