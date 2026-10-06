# gc-audit-text-between-markers-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `text_between_markers(...)` audit helper.
- [x] Replace repeated marker-based helper body slicing.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Repeated marker-based audit slicing uses one helper.
- [x] Missing marker behavior remains fail-closed via empty body.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
