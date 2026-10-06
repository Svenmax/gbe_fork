# gc-local-details-update-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add details update apply helper declaration and implementation.
- [x] Replace 7046 direct Local details writes.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] 7046 details Local writes use a named helper boundary.
- [x] Options and custom game application are tested.
- [x] Existing side-effect ordering remains handler-owned.
- [x] Full GC verification and diff check pass.
