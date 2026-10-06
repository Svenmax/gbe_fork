# gc-local-owner-name-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add owner name apply helper declaration and implementation.
- [x] Replace owner adoption direct owner_name assignments.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Owner adoption owner_name writes use a named helper boundary.
- [x] Matching and changed owner_name paths are tested.
- [x] Existing publish and diagnostic ordering remains coordinator-owned.
- [x] Full GC verification and diff check pass.
