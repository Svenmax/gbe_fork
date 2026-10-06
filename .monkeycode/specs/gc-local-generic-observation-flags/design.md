# gc-local-generic-observation-flags design

## Current State

Member coordinator writes generic lobby observation flags directly while deciding whether to wait for join confirmation, suppress kick detection, or suppress owner adoption logging.

## Design

- Add helpers for local member seen, waiting join confirmation logged, kicked suppression logged, and owner adoption suppression logged.
- Keep all decisions, logs, push/reset behavior, and publish behavior in the member coordinator.
- Use helper return values to preserve existing one-time log guards.
- Add focused state tests covering initial marks, duplicate suppression, and reset-on-seen behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
