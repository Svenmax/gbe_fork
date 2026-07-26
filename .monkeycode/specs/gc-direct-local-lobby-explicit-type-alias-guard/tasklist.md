# Tasklist: Direct Local Lobby Explicit Type Alias Guard

## Tasks

- [x] Extend audit 10d mutable alias matcher to include `GBE_LocalLobby`.
- [x] Add regression test for `GBE_LocalLobby& lobby = GBE_local_lobby`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
