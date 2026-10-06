# gc-local-broadcast-channel-apply design

## Current State

7149, 7367, and 8054 directly write broadcast channel fields on `GBE_local_lobby` before publishing shared lobby state and sending practice lobby details updates.

## Design

- Add `apply_broadcast_channel(...)` for complete join metadata replacement.
- Add `patch_broadcast_channel(...)` for 7367 optional-field update semantics.
- Add `clear_broadcast_channel(...)` for close semantics that records the request channel id and clears metadata strings.
- Replace only the broadcast channel field writes in the chat handler.
- Add focused lobby state tests for apply, no-op apply, optional patch preservation, no-op patch, and clear behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
