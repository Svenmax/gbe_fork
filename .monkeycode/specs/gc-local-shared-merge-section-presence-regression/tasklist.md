# gc-local-shared-merge-section-presence-regression tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Add a regression test for missing Local/shared merge §5.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Missing Local/shared merge §5 fail-fast behavior is covered by a regression test.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
