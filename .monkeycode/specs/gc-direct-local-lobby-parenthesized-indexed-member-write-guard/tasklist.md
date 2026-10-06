# GC Direct Local Lobby Parenthesized Indexed Member Write Guard Task List

## Tasks

- [x] Add audit 10d detection for parenthesized indexed member Local lobby writes.
- [x] Add regression test for `(GBE_local_lobby).members[0].team = team`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
