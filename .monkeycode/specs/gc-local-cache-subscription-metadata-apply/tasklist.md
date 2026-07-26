# gc-local-cache-subscription-metadata-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add cache subscription metadata apply helper declaration and implementation.
- [x] Replace direct CacheSubscribed cache metadata writes.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] CacheSubscribed metadata Local writes have a named helper boundary.
- [x] Missing optional fields still clear present flags and values.
- [x] Log, summary, and publish ordering remains coordinator-owned.
- [x] Full GC verification and diff check pass.
