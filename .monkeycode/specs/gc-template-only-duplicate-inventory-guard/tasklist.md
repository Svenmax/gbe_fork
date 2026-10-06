# gc-template-only-duplicate-inventory-guard tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Mark the slice as WIP in `docs/gc/ACTIVE_QUEUE.md`.
- [x] Add duplicate `TEMPLATE_ONLY` emsg detection to `audit_template_only_inventory_guard()`.
- [x] Add a duplicate template-only inventory regression test.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Duplicate `TEMPLATE_ONLY` inventory emsgs fail the audit.
- [x] Production C++ template replay behavior remains unchanged.
- [x] Full GC verification and diff check pass.
