# Requirements: Direct Local Lobby Mutating Method Write Guard

## Scope

Tighten audit 10d so direct mutating method calls on `GBE_local_lobby` fields cannot bypass the write guard.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `GBE_local_lobby.members.clear()`.
2. The audit shall reject common mutating container/string methods on direct Local lobby field chains.
3. Existing direct object, simple field, nested field, compound field, and target pointer diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
