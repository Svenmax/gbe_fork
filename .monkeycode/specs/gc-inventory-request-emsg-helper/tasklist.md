# gc-inventory-request-emsg-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `request_emsg` comparison extraction helper.
- [x] Replace local direct conditional fallback emsg extraction.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Direct conditional fallback `request_emsg` extraction uses one helper.
- [x] Existing comparison behavior and issue strings remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
