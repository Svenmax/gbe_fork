# gc-local-kick-member-snapshot-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add kick-member snapshot apply helper declaration and implementation.
- [x] Replace 7081 post-kick direct Local assignment.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] 7081 post-kick Local snapshot apply uses a named helper boundary.
- [x] Snapshot application is tested.
- [x] Existing side-effect ordering remains handler-owned.
- [x] Full GC verification and diff check pass.
