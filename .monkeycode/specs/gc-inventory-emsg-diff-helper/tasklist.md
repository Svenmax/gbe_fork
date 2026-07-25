# gc-inventory-emsg-diff-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared emsg set-diff issue helper.
- [x] Replace duplicated routing emsg mismatch diagnostics.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Routing emsg set mismatch diagnostics share one helper.
- [x] Existing comparison behavior and issue prefixes remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
