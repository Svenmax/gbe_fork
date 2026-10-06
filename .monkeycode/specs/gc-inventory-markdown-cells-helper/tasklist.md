# gc-inventory-markdown-cells-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `markdown_cells(...)` helper.
- [x] Replace repeated inventory table split expressions with the helper.
- [x] Add helper-level regression tests for current cell splitting behavior.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Inventory table row parsing is centralized for routing and Local/shared merge guards.
- [x] Existing inventory audit behavior remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
