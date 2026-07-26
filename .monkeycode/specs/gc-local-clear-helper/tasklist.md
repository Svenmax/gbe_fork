# gc-local-clear-helper tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add Local clear helper declaration and implementation.
- [x] Replace pure Local clear assignments.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Pure Local clear operations use a named helper boundary.
- [x] Plan apply object assignments remain unchanged.
- [x] Existing side-effect ordering remains coordinator-owned.
- [x] Full GC verification and diff check pass.
