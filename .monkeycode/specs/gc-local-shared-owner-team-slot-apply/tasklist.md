# gc-local-shared-owner-team-slot-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add owner team/slot apply helper declarations and implementations.
- [x] Reuse helpers from shared restore and 7034 draft owner apply path.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `bash tools/run_gc_verification.sh --full`.
- [x] Run `git diff --check`.

## Done Criteria

- [x] 7034 owner team/slot Local writes have named apply helpers.
- [x] Matching value no-op behavior is tested.
- [x] Draft owner slot zero guard remains in the handler.
- [x] Full GC verification and diff check pass.
