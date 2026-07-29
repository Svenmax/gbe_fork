# gc-inventory-section-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `inventory_section` helper.
- [x] Replace duplicated routing inventory section slicing.
- [x] Replace duplicated Local/shared merge inventory section slicing.
- [x] Add helper-level tests for next-heading and missing-heading behavior.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Inventory section slicing has one shared audit helper.
- [x] Existing consumer-specific failure messages remain at call sites.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
