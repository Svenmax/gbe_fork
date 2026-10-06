# Tasklist: Direct Local Lobby Mutable Alias Guard

## Tasks

- [x] Add audit 10d detection for mutable aliases to `GBE_local_lobby`.
- [x] Add regression test for `auto& lobby = GBE_local_lobby`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
