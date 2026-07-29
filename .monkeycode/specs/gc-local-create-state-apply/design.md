# Design: GC Local Create State Apply

## Approach

Add `apply_create_lobby_state_plan(GBE_LocalLobby &, const CreateLobbyStateApplyPlan &)` in `gbe::dota_lobby_state` and replace the direct 7038 create handler assignment with this helper.

## Behavior Preservation

- `compose_create_lobby_state_apply_plan(...)` remains the planner for the full create snapshot and side-effect flags.
- `apply_lobby_generation(...)` remains a separate write immediately after the snapshot apply.
- Normalize, reconnect, action execution, publish, and response order stays owned by `gbe_dota_lobby_create_handlers.cpp`.

## Tests

- Extend `gbe_dota_lobby_state_test` to assert that the helper applies the plan lobby snapshot and preserves plan members.
