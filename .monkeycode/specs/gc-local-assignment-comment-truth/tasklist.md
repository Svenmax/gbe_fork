# Task List: GC Local Assignment Comment Truth

## Implementation

- [x] Update stale create/join overview comments.
- [x] Update stale chat leave and broadcast overview comments.
- [x] Update `LOCAL_LOBBY_USAGE.md` complete Local snapshot write table.

## Verification

- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.
- [x] Confirm stale `GBE_local_lobby =` comments are gone.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Stale direct assignment comments are removed.
- [x] Production behavior is unchanged.
