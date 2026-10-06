# gc-local-shared-merge-generic-section-end tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Make Local/shared merge §5 end at any next level-2 heading.
- [x] Add a regression test for renumbered next heading.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Local/shared merge §5 parsing ignores rows after any following level-2 heading.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
