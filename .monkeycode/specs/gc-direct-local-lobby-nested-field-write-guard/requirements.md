# Requirements: Direct Local Lobby Nested Field Write Guard

## Scope

Tighten audit 10d so direct nested member writes on `GBE_local_lobby` cannot bypass the direct field write guard.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `GBE_local_lobby.custom_game.game_id = ...`.
2. The audit shall keep existing object, one-level field, and target pointer write diagnostics unchanged.
3. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
