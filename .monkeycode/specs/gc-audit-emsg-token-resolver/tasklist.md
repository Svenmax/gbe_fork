# gc-audit-emsg-token-resolver tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `resolve_emsg_token(...)` audit helper.
- [x] Replace registry inventory inline token resolution.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Registry inventory emsg token resolution uses one helper.
- [x] Unknown token diagnostics remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
