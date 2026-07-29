# gc-inventory-sorted-numeric-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `sorted_numeric_values(...)` helper.
- [x] Replace hand-written numeric sort calls in inventory audits.
- [x] Add helper-level regression tests for numeric sorting behavior.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Numeric emsg sorting is centralized for inventory guards.
- [x] Existing audit behavior remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
