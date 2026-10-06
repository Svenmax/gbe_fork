# Requirements: Direct Local Lobby Indexed Member Write Guard

## Scope

Tighten audit 10d so direct writes to members of subscripted `GBE_local_lobby` fields cannot bypass the field write guard.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `GBE_local_lobby.members[0].team = team`.
2. Existing indexed object, simple field, prefix field, mutating method, target pointer, and alias diagnostics shall remain unchanged.
3. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
