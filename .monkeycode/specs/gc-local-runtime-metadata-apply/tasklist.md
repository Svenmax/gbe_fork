# gc-local-runtime-metadata-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add runtime metadata helper declaration and implementation.
- [x] Replace generic metadata publish direct connect/server id writes.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Generic metadata publish connect/server id Local writes use a named helper boundary.
- [x] Empty connect and zero server id application are tested.
- [x] Publish coordinator side-effect ordering remains handler-owned.
- [x] Full GC verification and diff check pass.
