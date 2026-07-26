# gc-local-broadcast-channel-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add broadcast channel apply, patch, and clear helper declarations and implementations.
- [x] Replace 7149, 7367, and 8054 direct broadcast channel writes.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Broadcast channel Local writes have named helper boundaries.
- [x] 7367 optional metadata semantics remain helper-owned and tested.
- [x] Publish/details/ack ordering remains handler-owned.
- [x] Full GC verification and diff check pass.
