# Tasklist: Direct Local Lobby Decltype Auto Alias Guard

## Tasks

- [x] Add audit 10d matcher for parenthesized `decltype(auto)` Local alias.
- [x] Add regression test for `decltype(auto) lobby = (GBE_local_lobby)`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
