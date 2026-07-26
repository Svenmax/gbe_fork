# Tasklist: Direct Local Lobby Parenthesized Field Write Guard

## Tasks

- [x] Add audit 10d detection for parenthesized direct Local lobby field writes.
- [x] Add regression test for `(GBE_local_lobby).state = 1u`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
