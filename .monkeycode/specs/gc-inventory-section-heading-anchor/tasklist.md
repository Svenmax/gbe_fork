# gc-inventory-section-heading-anchor tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Anchor `inventory_section(...)` heading lookup to line start.
- [x] Add helper-level regression test for inline heading text before the real heading.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Inline prose cannot satisfy inventory section heading lookup.
- [x] Existing next-heading section-end behavior remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
