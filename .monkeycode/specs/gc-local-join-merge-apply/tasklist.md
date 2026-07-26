# Task List: GC Local Join Merge Apply

## Implementation

- [x] Add `apply_join_lobby_merge_plan(...)` declaration and implementation.
- [x] Replace the direct 7044 join handler Local assignment with the helper.
- [x] Add helper-level regression coverage.
- [x] Update Local/shared inventory and current GC docs.

## Verification

- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Routing truth table remains unchanged.
- [x] Production response, push, publish, and deferred side-effect order is preserved.
