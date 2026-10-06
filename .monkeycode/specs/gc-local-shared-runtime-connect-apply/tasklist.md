# gc-local-shared-runtime-connect-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add `apply_runtime_connect(...)` declaration and implementation.
- [x] Replace 4508 direct runtime connect field write.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `bash tools/run_gc_verification.sh --full`.
- [x] Run `git diff --check`.

## Done Criteria

- [x] 4508 runtime connect Local write has a named apply helper.
- [x] Empty input preserve behavior is tested.
- [x] Shared Store compare_update timing remains unchanged.
- [x] Full GC verification and diff check pass.
