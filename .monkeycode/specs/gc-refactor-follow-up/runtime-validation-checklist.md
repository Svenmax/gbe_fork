# GC Follow-Up Runtime Validation Checklist

This checklist records live-client or integration validation scenarios that complement offline GC tests. Use it when a Premake-capable or client-capable environment is available.

## Validation Rules

- Run `tools/run_gc_verification.sh` before live validation.
- Record the platform, build source, and Dota client version when a live validation run is performed.
- Capture the relevant GC debug log lines and the visible client outcome for each scenario.
- If live validation is unavailable, keep the scenario marked as pending and rely on the listed offline coverage.

## Scenarios

### 7035 Abandon Current Game

Goal: validate current-game abandon teardown behavior after decision/helper refactors.

Offline coverage:
- `test_lobby_abandon_current_game_disconnect_queues_25`
- `test_lobby_abandon_ready_teardown_queues_postgame_response`
- `gbe_dota_lobby_state_test` abandon decision cases

Live validation steps:
1. Start an active Dota custom game lobby and reach current-game state.
2. Trigger abandon current game from the client.
3. Confirm the client receives the expected cache unsubscribe and teardown flow.
4. Confirm pending reset/finalize behavior does not loop or suppress unrelated lobbies.

Status: pending live validation.

### 7004 Signout

Goal: validate normal signout finalize behavior and pending normal signout state clearing.

Offline coverage:
- `test_lobby_normal_signout_pending_clear_resets_state`
- `gbe_dota_lobby_state_test` teardown retrieval decision for 25 finalize

Live validation steps:
1. Enter a Dota lobby or custom game state with GC session active.
2. Trigger normal signout.
3. Confirm the client receives cache unsubscribe and follow-up finalize behavior.
4. Confirm repeated retrieve/message polling does not repeat stale normal signout finalize state.

Status: pending live validation.

### 8052 And 8053 Custom Game Loading

Goal: validate started-loading and finished-loading lifecycle behavior after DTO and launch decision refactors.

Offline coverage:
- `test_match_started_loading_updates_custom_game_before_publish`
- `test_match_finished_loading_marks_loaded_before_publish`
- `test_match_finished_loading_failure_preserves_reason`
- `gbe_dota_lobby_state_test` launch lifecycle decision cases

Live validation steps:
1. Launch a custom game lobby through ready-up.
2. Observe started-loading transition and details update behavior.
3. Observe finished-loading success transition and local member loaded state.
4. Repeat a failed loading path if the client can trigger a non-success result.

Status: pending live validation.

### 2569 Equip Items Full Forward

Goal: validate item equip side-effect order with persistence, server GC forward, network broadcast, and snapshot refresh.

Offline coverage:
- `test_inventory_equip_basic`
- `test_inventory_equip_full_forward`
- equip planner focused tests for empty, missing item, style bitmask, and multi-item order

Live validation steps:
1. Equip one or more items in a Dota lobby or active session.
2. Confirm local response arrives before external propagation side effects.
3. Confirm item persistence occurs once per successful equip request.
4. Confirm peer clients or server GC consumers observe the updated equipped items.

Status: pending live validation.

### Reconnect And Direct Connect Flow

Goal: validate reconnect eligibility, recent context fallback, and direct connect interception behavior.

Offline coverage:
- `gbe_dota_lobby_state_test` reconnect eligibility and interception decision cases
- reconnect accessor coverage through payload helper and handler stubs

Live validation steps:
1. Start an arcade/custom game flow that produces a server id and connect string.
2. Trigger reconnect eligibility through the expected auth or connection lifecycle.
3. Confirm direct connect interception consumes eligibility once.
4. Confirm stale or mismatched reconnect contexts do not trigger direct connect.

Status: pending live validation.

## Completion Criteria

Each scenario is complete when the live validation status records the platform, build source, client version, observed GC log evidence, and user-visible result. If live validation remains unavailable, keep the scenario pending and record the latest offline verification command that covers the path.
