# gc-local-shared-sourcetv-metadata-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add `apply_source_tv_metadata(...)` declaration and implementation.
- [x] Replace 4508 direct SourceTV metadata field writes.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `bash tools/run_gc_verification.sh --full`.
- [x] Run `git diff --check`.

## Done Criteria

- [x] SourceTV metadata Local field group has a named apply helper.
- [x] Zero input preserve behavior is tested.
- [x] 4508 publish timing remains unchanged.
- [x] Full GC verification and diff check pass.
