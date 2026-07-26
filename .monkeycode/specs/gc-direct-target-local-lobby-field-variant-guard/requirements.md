# Requirements: Direct Target Local Lobby Field Variant Guard

## Scope

Lock audit 10d coverage for target pointer field-write variants on `client_target->GBE_local_lobby`.

## Requirements

1. The audit test suite shall explicitly cover nested target field writes shaped like `options.client_target->GBE_local_lobby.custom_game.game_id = ...`.
2. The audit test suite shall explicitly cover target compound writes shaped like `options.client_target->GBE_local_lobby.generation += ...`.
3. The audit test suite shall explicitly cover target mutating method calls shaped like `options.client_target->GBE_local_lobby.members.clear()`.
4. The change shall not alter production Dota GC behavior or audit implementation semantics.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
