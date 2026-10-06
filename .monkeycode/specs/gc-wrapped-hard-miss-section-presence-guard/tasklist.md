# gc-wrapped-hard-miss-section-presence-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Make missing fallback §2 fail the wrapped hard miss audit.
- [x] Add a regression test for missing fallback §2.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Missing fallback §2 does not fall back to whole-document parsing for wrapped hard miss inventory.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
