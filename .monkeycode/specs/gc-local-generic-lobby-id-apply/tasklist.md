# gc-local-generic-lobby-id-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add generic_lobby_id apply helper declaration and implementation.
- [x] Make restore helper reuse the apply helper.
- [x] Replace direct generic_lobby_id writes in create, lifecycle leave fallback, and leave-generic cleanup.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] generic_lobby_id Local writes use a named helper boundary.
- [x] Update and zero clear semantics are tested.
- [x] Existing side-effect ordering remains handler/coordinator-owned.
- [x] Full GC verification and diff check pass.
