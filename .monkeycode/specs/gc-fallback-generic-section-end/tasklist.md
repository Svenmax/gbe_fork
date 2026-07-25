# gc-fallback-generic-section-end tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Make fallback §2 end at any next level-2 heading.
- [x] Add regression tests for missing separator after fallback §2.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Fallback §2 parsing ignores rows after any following level-2 heading.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
