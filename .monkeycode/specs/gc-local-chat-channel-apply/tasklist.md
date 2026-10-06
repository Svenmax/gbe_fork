# gc-local-chat-channel-apply tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Add chat channel apply and clear helper declarations and implementations.
- [x] Replace 7009 join and 7272 leave direct chat channel writes.
- [x] Add focused lobby state tests.
- [x] Update `docs/gc/LOCAL_LOBBY_USAGE.md`, `docs/gc/CURRENT.md`, and `docs/gc/ACTIVE_QUEUE.md`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Chat channel Local writes have named helper boundaries.
- [x] Generated channel id reuse behavior remains handler-owned.
- [x] Broadcast channel handling remains unchanged.
- [x] Full GC verification and diff check pass.
