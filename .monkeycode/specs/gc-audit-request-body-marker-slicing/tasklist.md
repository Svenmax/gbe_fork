# gc-audit-request-body-marker-slicing tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Replace direct request body marker slicing.
- [x] Replace wrapped request body marker slicing.
- [x] Confirm existing helper-level regression coverage is sufficient.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Direct and wrapped request body audit slicing use the shared marker helper.
- [x] Missing marker behavior remains fail-closed via empty body.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
