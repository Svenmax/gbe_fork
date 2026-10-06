# Tasklist: Direct Local Lobby Pointer Alias Guard

## Tasks

- [x] Add regression test for `GBE_DotaLobbyState* lobby = &GBE_local_lobby`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code and audit implementation remain unchanged.
