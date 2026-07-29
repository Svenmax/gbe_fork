# gc-local-server-id-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add server_id apply helper declaration and implementation.
- [x] Make runtime metadata helper reuse the server_id helper.
- [x] Replace recover coordinator direct server_id write.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Recover coordinator server_id Local write uses a named helper boundary.
- [x] Update and zero clear semantics are tested.
- [x] Existing side-effect ordering remains recover-coordinator-owned.
- [x] Full GC verification and diff check pass.
