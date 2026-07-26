# Requirements: Direct Local Lobby Prefix Inc/Dec Guard

## Scope

Tighten audit 10d so prefix increment/decrement writes on `GBE_local_lobby` fields cannot bypass the field write guard.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `++GBE_local_lobby.generation`.
2. The audit shall reject production `.cpp` writes shaped like `--GBE_local_lobby.generation`.
3. Existing direct object, simple field, compound field, mutating method, target pointer, and alias diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
