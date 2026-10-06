# gc-local-owner-name-apply design

## Current State

Generic lobby owner adoption assigns `GBE_local_lobby.owner_name` directly after updating owner identity and member state.

## Design

- Add `apply_lobby_owner_name(GBE_LocalLobby &, const std::string &)`.
- Return `false` for matching values and `true` after assigning changed values.
- Replace only the two owner adoption name assignments.
- Keep owner adoption, local owner publish, metadata publish, and logging in the coordinator.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
