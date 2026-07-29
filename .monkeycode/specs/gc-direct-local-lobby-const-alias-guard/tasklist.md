# Tasklist: Direct Local Lobby Const Alias Guard

## Tasks

- [x] Refine audit 10d mutable alias matcher to skip `const` aliases.
- [x] Add acceptance regression for `const auto& lobby = GBE_local_lobby`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
