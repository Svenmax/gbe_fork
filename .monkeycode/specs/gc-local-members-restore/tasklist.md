# gc-local-members-restore tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add members restore helper declaration and implementation.
- [x] Replace shared-to-local runtime restore direct members assignment.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Runtime restore members writes use a named helper boundary.
- [x] Matching and changed member restore paths are tested.
- [x] Existing post-restore side-effect ordering remains coordinator-owned.
- [x] Full GC verification and diff check pass.
