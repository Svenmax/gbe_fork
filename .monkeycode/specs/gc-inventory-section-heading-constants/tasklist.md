# gc-inventory-section-heading-constants tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add named inventory section heading constants.
- [x] Replace direct inventory heading literals at lookup call sites.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Inventory section lookup headings are centralized as named constants.
- [x] Existing missing-section diagnostics remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
