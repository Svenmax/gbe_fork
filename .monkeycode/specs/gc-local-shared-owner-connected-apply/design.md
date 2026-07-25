# gc-local-shared-owner-connected-apply design

## Current State

`GBE_SetDotaLobbyMemberConnected` directly writes `GBE_local_lobby.owner_connected` when the target Steam ID is the lobby owner, then aggregates that change with member connected updates.

## Design

- Add `apply_lobby_owner_connected(GBE_LocalLobby &, bool)` to `gbe_dota_lobby_state`.
- Return true only when `owner_connected` changes.
- Reuse the helper from `restore_lobby_owner_connected(...)` to keep restore and local apply semantics aligned.
- Replace the launch coordinator owner branch direct write with the helper.
- Add focused lobby state tests for matching no-op and changed owner connected values.

## Validation

- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
