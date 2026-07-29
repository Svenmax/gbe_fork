# Requirements: GC Local Client Lobby Restore Snapshot Apply

## Scope

Centralize the custom game lifecycle cross-GC client Local lobby restore snapshot write behind a named state helper while preserving production protocol order.

## Requirements

- The helper shall apply the full client restore `GBE_LocalLobby` snapshot to the target client Local lobby.
- The custom game lifecycle coordinator shall retain the client target guard and active/lobby_id restore guard.
- Launch peripheral reset, client Local snapshot restore, last launch state clear, and local coordinator reset shall keep their existing order.
- The change shall not modify routing inventory or shared Store generation gate behavior.
