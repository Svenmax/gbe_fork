# Tasklist: Direct Local Lobby Indexed Member Write Guard

## Tasks

- [x] Add audit 10d detection for indexed Local member writes.
- [x] Add regression test for `GBE_local_lobby.members[0].team = team`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
