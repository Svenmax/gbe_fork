# gc-local-bot-difficulty-team-apply design

## Current State

7047 set team slot directly writes `GBE_local_lobby.bot_difficulty_dire` or `GBE_local_lobby.bot_difficulty_radiant` based on the request team or current owner team.

## Design

- Add `apply_lobby_bot_difficulty_for_team(GBE_LocalLobby &, std::uint32_t, std::uint32_t)`.
- Keep `has_bot_difficulty` and bot team derivation in the handler.
- Use existing Dota team classification for radiant/dire field selection.
- Add focused tests for radiant update, dire update, field preservation, and no-op behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
