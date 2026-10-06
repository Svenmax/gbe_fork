# gc-legacy-wrapped-parser-section-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Limit legacy parser inventory parsing to §5.
- [x] Add a regression test for marker text outside §5.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] `LEGACY_UNUSED` marker text outside parser §5 does not satisfy the audit.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
