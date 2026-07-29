# gc-template-only-section-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Limit template-only inventory parsing to §4.
- [x] Add a regression test for a `TEMPLATE_ONLY` row outside §4.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] `TEMPLATE_ONLY` rows outside template replay §4 do not satisfy the audit.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
