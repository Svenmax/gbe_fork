# gc-local-clear-helper design

## Current State

Several coordinators clear Local lobby state through direct `GBE_local_lobby = {}` or `GBE_LocalLobby{}` assignments.

## Design

- Add `clear_local_lobby(GBE_LocalLobby &)` to the state helper surface.
- Replace pure Local clear assignments in runtime clear, postgame 7272 stale shared cleanup, and recover generation exhausted handling.
- Keep object replacement from create/join/launch plans unchanged.
- Add focused state test coverage for clearing active state, lobby id, and members.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
