# gc-inventory-duplicate-issues-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `append_duplicate_issues(...)` helper.
- [x] Replace repeated routing duplicate issue loops with the helper.
- [x] Add helper-level regression tests for duplicate issue emission.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Duplicate issue emission is centralized for routing inventory guards.
- [x] Existing duplicate issue message text remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
