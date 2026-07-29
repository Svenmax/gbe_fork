# Tasklist: Direct Local Lobby Nested Field Write Guard

## Tasks

- [x] Extend audit 10d field-write regex to include nested member chains.
- [x] Add regression test for `GBE_local_lobby.custom_game.game_id = ...`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
