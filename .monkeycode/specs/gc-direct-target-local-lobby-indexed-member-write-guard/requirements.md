# Requirements: Direct Target Local Lobby Indexed Member Write Guard

## Scope

Add explicit audit 10d coverage for target-client writes to members of subscripted `GBE_local_lobby` fields.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `options.client_target->GBE_local_lobby.members[0].team = team`.
2. Existing direct target object, field, indexed field, compound, prefix, and mutating method diagnostics shall remain unchanged.
3. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
