# gc-audit-word-token-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `contains_word_token(...)` audit helper.
- [x] Replace retired lifecycle token checks.
- [x] Replace retired reconnect token checks.
- [x] Replace retired shared-lobby compatibility token checks.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Same-shape whole-word token detection uses one helper in targeted audits.
- [x] Existing issue strings remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
