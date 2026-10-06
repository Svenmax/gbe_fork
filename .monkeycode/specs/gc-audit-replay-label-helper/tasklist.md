# gc-audit-replay-label-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add shared `extract_replay_fixture_labels(...)` audit helper.
- [x] Replace post-login dispatch inline replay label parsing.
- [x] Add helper-level regression tests.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Replay fixture label parsing uses one helper.
- [x] Existing fixture diagnostics remain unchanged.
- [x] Production C++ behavior remains unchanged.
- [x] Full GC verification and diff check pass.
