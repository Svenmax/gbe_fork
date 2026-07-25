# gc-wrapped-hard-miss-section-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Limit wrapped hard miss inventory parsing to fallback §2.
- [x] Add a regression test for marker text outside §2.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] HARD_MISS marker text outside fallback §2 does not satisfy the audit.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
