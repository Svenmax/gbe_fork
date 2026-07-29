# Requirements: Direct Local Lobby Indexed Prefix Write Guard

## Scope

Tighten audit 10d so prefix increment/decrement writes on indexed `GBE_local_lobby` fields cannot bypass the field write guard.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `++GBE_local_lobby.members[0].team`.
2. The audit shall reject production `.cpp` writes shaped like `++GBE_local_lobby.members[0]`.
3. Existing direct object, simple field, indexed field, indexed member, target pointer, and alias diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
