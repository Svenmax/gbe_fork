# gc-audit-function-call-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `contains_function_call(...)` audit helper.
- [x] Replace same-shape lifecycle and reconnect call checks.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Same-shape function call detection uses one helper in the targeted audits.
- [x] Existing issue strings remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
