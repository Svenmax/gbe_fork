# gc-audit-any-token-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `contains_any_token(...)` audit helper.
- [x] Replace wrapped hard-miss helper body prohibited route token check.
- [x] Replace wrapped hard-miss request body prohibited route token check.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Same-shape any-token checks use one helper in targeted wrapped audit.
- [x] Substring membership semantics remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
