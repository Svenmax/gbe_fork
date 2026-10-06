# Requirements: Direct Target Local Lobby Prefix Inc/Dec Guard

## Scope

Add explicit audit 10d coverage for prefix increment/decrement writes on target-client `GBE_local_lobby` fields.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `++options.client_target->GBE_local_lobby.generation`.
2. The audit shall reject production `.cpp` writes shaped like `--options.client_target->GBE_local_lobby.generation`.
3. Existing direct Local lobby diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
