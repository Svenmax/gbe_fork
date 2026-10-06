# Requirements: Direct Local Lobby Compound Field Write Guard

## Scope

Tighten audit 10d so direct compound writes on `GBE_local_lobby` fields cannot bypass the field write guard.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `GBE_local_lobby.generation += ...`.
2. The audit shall reject production `.cpp` writes shaped like `GBE_local_lobby.state++` or `GBE_local_lobby.state--`.
3. Existing direct object, simple field, nested field, and target pointer diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
