# gc-local-owner-team-slot-handler-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Replace 7047 owner team direct write with `apply_lobby_owner_team(...)`.
- [x] Replace 7047 owner slot direct write with `apply_lobby_owner_slot(...)`.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] 7047 owner team/slot Local writes use named helper boundaries.
- [x] Member update and bot difficulty handling remain handler-owned.
- [x] Publish/details/ack ordering remains unchanged.
- [x] Full GC verification and diff check pass.
