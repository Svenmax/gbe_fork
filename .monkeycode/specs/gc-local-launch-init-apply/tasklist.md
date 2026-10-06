# Task List: GC Local Launch Init Apply

## Implementation

- [x] Add `apply_launch_init_plan(...)` declaration and implementation.
- [x] Replace the direct 7041 lifecycle handler Local assignment with the helper.
- [x] Add helper-level regression coverage.
- [x] Update Local/shared inventory and current GC docs.

## Verification

- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Routing truth table remains unchanged.
- [x] Production response, push, publish, and deferred side-effect order is preserved.
