# Task List: GC Local Create State Apply

## Implementation

- [x] Add `apply_create_lobby_state_plan(...)` declaration and implementation.
- [x] Replace the direct 7038 create handler Local assignment with the helper.
- [x] Add helper-level regression coverage.
- [x] Update Local/shared inventory and current GC docs.

## Verification

- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Routing truth table remains unchanged.
- [x] Production response, push, publish, and deferred side-effect order is preserved.
