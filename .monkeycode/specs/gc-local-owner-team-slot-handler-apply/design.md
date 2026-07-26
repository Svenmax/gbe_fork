# gc-local-owner-team-slot-handler-apply design

## Current State

7047 set team slot directly writes `GBE_local_lobby.owner_team` and `GBE_local_lobby.owner_slot` when the local user is the lobby owner. The 7034 draft owner path and shared restore path already use the owner team/slot apply helpers.

## Design

- Replace only the local owner 7047 direct writes with `apply_lobby_owner_team(...)` and `apply_lobby_owner_slot(...)`.
- Keep request presence guards in the handler.
- Keep member update, bot difficulty calculation, arcade slot normalization, Local member data publish, shared publish, details update, and ack push order unchanged.
- Rely on existing helper-level owner team/slot tests for idempotence and value update behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
