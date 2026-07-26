# gc-local-custom-game-loading-metadata-apply design

## Current State

8052 started loading directly writes `GBE_local_lobby.custom_game.game_id` and `GBE_local_lobby.game_start_time` when request values are non-zero, then computes launch setup and executes the lifecycle decision.

## Design

- Add `apply_custom_game_loading_metadata(GBE_LocalLobby &, std::uint64_t, std::uint32_t)`.
- Keep non-zero preservation semantics inside the helper.
- Replace only the two Local field writes in the 8052 handler.
- Add focused tests for absent values, changed values, and matching no-op behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
