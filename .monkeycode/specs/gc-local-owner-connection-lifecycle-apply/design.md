# gc-local-owner-connection-lifecycle-apply design

## Current State

Connection lifecycle owner reconnect and disconnect paths directly write `GBE_local_lobby.owner_connected`. The launch member connection path and shared restore path already use `apply_lobby_owner_connected(...)`.

## Design

- Replace the owner reconnect direct write with `apply_lobby_owner_connected(GBE_local_lobby, true)` and keep publishing only when the helper reports a change.
- Replace the owner disconnect direct write with `apply_lobby_owner_connected(GBE_local_lobby, false)` and keep the existing publish decision controlled by `postgame_suppress_publish`.
- Keep member connected handling on `GBE_SetDotaLobbyMemberConnected(...)`.
- Rely on the existing helper-level tests for owner connected idempotence.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
