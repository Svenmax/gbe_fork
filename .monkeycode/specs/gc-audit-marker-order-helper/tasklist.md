# gc-audit-marker-order-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `marker_appears_after(...)` audit helper.
- [x] Replace direct routing marker order checks.
- [x] Replace wrapped routing marker order checks.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Same-shape marker order checks use one helper in targeted audits.
- [x] Missing marker behavior remains unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
