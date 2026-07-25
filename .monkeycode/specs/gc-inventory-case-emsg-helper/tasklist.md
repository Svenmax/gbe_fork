# gc-inventory-case-emsg-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `case` emsg extraction helper.
- [x] Replace duplicated template replay `case` parsing.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Template replay `case` emsg extraction uses one helper.
- [x] Existing comparison behavior and issue strings remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
