# Tasklist: Direct Target Local Lobby Indexed Field Write Guard

## Tasks

- [x] Add regression test for `options.client_target->GBE_local_lobby.members[0] = member`.
- [x] Keep audit implementation unchanged.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Production code and audit implementation remain unchanged.
