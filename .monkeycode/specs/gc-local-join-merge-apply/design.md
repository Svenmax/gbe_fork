# Design: GC Local Join Merge Apply

## Approach

Add `apply_join_lobby_merge_plan(GBE_LocalLobby &, const JoinLobbyMergePlan &)` in `gbe::dota_lobby_state` and replace the direct 7044 join handler assignment with this helper.

## Behavior Preservation

- `join_lobby_merge_plan_from_context(...)` remains the source of the full join snapshot.
- `apply_lobby_generation(...)` remains separate and immediately follows the snapshot apply.
- Matched generic lobby join/sync and the following action loop stay owned by `gbe_dota_lobby_join_handlers.cpp`.

## Tests

- Extend `gbe_dota_lobby_state_test` to assert that the helper applies the join plan lobby snapshot and preserves plan members.
