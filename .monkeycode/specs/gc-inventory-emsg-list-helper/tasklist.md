# gc-inventory-emsg-list-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Replace routing inventory emsg set diagnostic sorting with `sorted_numeric_values(...)`.
- [x] Replace routing inventory emsg iteration sorting with `sorted_numeric_values(...)`.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Routing inventory emsg diagnostics use numeric ordering consistently.
- [x] Existing routing inventory comparison behavior remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
