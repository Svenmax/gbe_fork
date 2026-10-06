# gc-inventory-multi-numeric-cell-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared multi numeric Markdown cell helper.
- [x] Replace template-only inline multi-emsg cell parsing.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Multi-emsg inventory cell parsing uses one helper.
- [x] Existing inventory parsing behavior and issue strings remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
