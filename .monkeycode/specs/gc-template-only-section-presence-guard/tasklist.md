# gc-template-only-section-presence-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Make missing template replay §4 fail the audit.
- [x] Add a regression test for missing template replay §4.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Missing template replay §4 does not fall back to whole-document parsing.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
