# gc-local-owner-connection-lifecycle-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Replace owner reconnect direct write with `apply_lobby_owner_connected(...)`.
- [x] Replace owner disconnect direct write with `apply_lobby_owner_connected(...)`.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Connection lifecycle owner connected Local writes use the named helper boundary.
- [x] Member connected handling remains unchanged.
- [x] Suppress guard and postgame publish suppression remain coordinator-owned.
- [x] Full GC verification and diff check pass.
