# Requirements: Direct Target Local Lobby Indexed Field Write Guard

## Scope

Add explicit audit 10d coverage for target-client subscript writes on `GBE_local_lobby` fields.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `options.client_target->GBE_local_lobby.members[0] = member`.
2. Existing direct target object, field, compound, prefix, and mutating method diagnostics shall remain unchanged.
3. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
