# gc-inventory-duplicate-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `record_duplicate(...)` helper.
- [x] Replace repeated inventory duplicate tracking with the helper.
- [x] Add helper-level regression tests for duplicate tracking behavior.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Inventory duplicate tracking is centralized for routing guards.
- [x] Existing duplicate guard behavior remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
