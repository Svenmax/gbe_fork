# gc-local-custom-game-loading-metadata-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add custom game loading metadata helper declaration and implementation.
- [x] Replace 8052 direct custom game id and start time writes.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] 8052 custom game loading metadata Local writes use a named helper boundary.
- [x] Zero-value preservation is tested.
- [x] Lifecycle decision ordering remains handler-owned.
- [x] Full GC verification and diff check pass.
