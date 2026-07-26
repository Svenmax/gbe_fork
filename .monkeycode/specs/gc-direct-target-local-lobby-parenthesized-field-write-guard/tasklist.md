# GC Direct Target Local Lobby Parenthesized Field Write Guard Task List

## Tasks

- [x] Extend audit 10d parenthesized field write detection to include target Local lobby objects.
- [x] Add regression test for `(options.client_target->GBE_local_lobby).state = 1u`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
