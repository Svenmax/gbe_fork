# Tasklist: Direct Local Lobby Compound Field Write Guard

## Tasks

- [x] Extend audit 10d field-write operator matching to compound assignment and inc/dec.
- [x] Add regression test for `GBE_local_lobby.generation += ...` and `GBE_local_lobby.state++`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
