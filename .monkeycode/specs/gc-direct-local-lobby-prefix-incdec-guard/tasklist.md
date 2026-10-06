# Tasklist: Direct Local Lobby Prefix Inc/Dec Guard

## Tasks

- [x] Add audit 10d detection for prefix increment/decrement on Local lobby fields.
- [x] Add regression test for `++GBE_local_lobby.generation`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code remains unchanged.
