# gc-registry-section-presence-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Make missing registry §1 fail the registry inventory audit.
- [x] Add a regression test for missing registry §1.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Missing registry §1 does not fall back to whole-document parsing for registry inventory.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
