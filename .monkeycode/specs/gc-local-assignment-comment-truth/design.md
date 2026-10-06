# Design: GC Local Assignment Comment Truth

## Approach

Update only comment/documentation text in files that still mention stale direct `GBE_local_lobby` object writes.

## Behavior Preservation

- No C++ executable statements change.
- Routing and Store truth tables remain unchanged.

## Verification

- Run `git diff --check`.
- Run `bash tools/run_gc_verification.sh --full`.
- Confirm grep for `GBE_local_lobby\s*=` only returns no stale production assignment comments.
