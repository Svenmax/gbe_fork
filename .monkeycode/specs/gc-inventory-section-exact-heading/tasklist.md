# gc-inventory-section-exact-heading tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Require exact heading-line match in `inventory_section(...)`.
- [x] Add helper-level regression tests for same-prefix longer heading and parenthetical heading annotation.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Same-prefix longer headings cannot satisfy inventory section heading lookup, while documented parenthetical annotations still match.
- [x] Existing next-heading section-end behavior remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
