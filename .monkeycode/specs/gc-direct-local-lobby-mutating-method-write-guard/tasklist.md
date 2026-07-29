# Tasklist: Direct Local Lobby Mutating Method Write Guard

## Tasks

- [x] Add audit 10d detection for direct mutating method calls on Local lobby fields.
- [x] Add regression test for `GBE_local_lobby.members.clear()`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
