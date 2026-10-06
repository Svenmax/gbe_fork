# gc-direct-conditional-duplicate-inventory-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Add duplicate `CONDITIONAL_*` emsg detection to `audit_direct_conditional_fallback_routing()`.
- [x] Add a duplicate direct conditional inventory regression test.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Duplicate direct conditional inventory rows fail the audit.
- [x] Production C++ fallback behavior remains unchanged.
- [x] Full GC verification and diff check pass.
