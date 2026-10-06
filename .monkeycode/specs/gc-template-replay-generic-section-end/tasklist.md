# gc-template-replay-generic-section-end tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Make template_replay §4 end at any next level-2 heading.
- [x] Add regression tests for missing separator after template_replay §4.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Template replay §4 parsing ignores rows after any following level-2 heading.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
