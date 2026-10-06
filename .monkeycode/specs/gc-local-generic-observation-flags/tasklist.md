# gc-local-generic-observation-flags tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add observation flag helper declarations and implementations.
- [x] Replace member coordinator direct observation flag assignments.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Generic lobby observation flags use named helper boundaries.
- [x] Single-shot log guard behavior is helper-tested.
- [x] Existing kick/adoption side-effect ordering remains coordinator-owned.
- [x] Full GC verification and diff check pass.
