# gc-local-shared-owner-connected-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add `apply_lobby_owner_connected(...)` declaration and implementation.
- [x] Reuse helper from shared restore and launch owner-connected apply path.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `bash tools/run_gc_verification.sh --full`.
- [x] Run `git diff --check`.

## Done Criteria

- [x] Launch owner-connected Local write has a named apply helper.
- [x] Matching value no-op behavior is tested.
- [x] Member connected changed aggregation remains unchanged.
- [x] Full GC verification and diff check pass.
