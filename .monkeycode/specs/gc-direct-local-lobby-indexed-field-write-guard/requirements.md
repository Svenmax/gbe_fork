# Requirements: Direct Local Lobby Indexed Field Write Guard

## Scope

Tighten audit 10d so direct subscript writes on `GBE_local_lobby` fields cannot bypass the field write guard.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `GBE_local_lobby.members[0] = member`.
2. The audit shall reject compound and postfix writes on indexed Local fields.
3. Existing direct object, simple field, prefix field, mutating method, target pointer, and alias diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
