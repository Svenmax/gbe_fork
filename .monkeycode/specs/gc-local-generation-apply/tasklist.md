# gc-local-generation-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add generation apply helper declaration and implementation.
- [x] Make restore helper reuse the apply helper.
- [x] Replace direct generation writes in create, join, runtime reset, lifecycle clear, and recover.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] generation Local writes use a named helper boundary.
- [x] No-op and update semantics are tested.
- [x] Existing generation sequencing remains caller-owned.
- [x] Full GC verification and diff check pass.
