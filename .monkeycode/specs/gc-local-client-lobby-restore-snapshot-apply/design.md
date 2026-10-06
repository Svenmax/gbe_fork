# Design: GC Local Client Lobby Restore Snapshot Apply

## Approach

Add `apply_client_lobby_restore_snapshot(GBE_LocalLobby &, const GBE_LocalLobby &)` in `gbe::dota_lobby_state` and replace the direct cross-GC client restore assignment in the lifecycle action executor.

## Behavior Preservation

- The coordinator still decides when a client target can be mirrored.
- The helper only owns the Local snapshot write.
- Launch peripheral reset and last launch state clear remain ordered around the write in `gbe_dota_custom_game_lifecycle_coordinator.cpp`.

## Tests

- Extend `gbe_dota_lobby_state_test` to assert that the helper applies a full Local restore snapshot.
