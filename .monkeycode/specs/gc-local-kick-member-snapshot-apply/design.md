# gc-local-kick-member-snapshot-apply design

## Current State

7081 prepares a Local lobby snapshot with the target member removed, calls the generic lobby kick API, then writes the snapshot back to Local after the kick succeeds.

## Design

- Add `apply_lobby_member_kick_snapshot(GBE_LocalLobby &, const GBE_LocalLobby &)`.
- Replace only the post-kick Local object assignment.
- Keep kick call, failure handling, shared publish, details update, and logging in the slot handler.
- Add focused state test coverage for full snapshot and member list application.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
