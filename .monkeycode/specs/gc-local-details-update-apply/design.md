# gc-local-details-update-apply design

## Current State

7046 set details directly assigns Local option fields in the handler, then applies custom game fields through a helper and runs normalization before publishing the details update.

## Design

- Add `apply_lobby_details_update(GBE_LocalLobby &, const DotaPracticeLobbyDetailsRequest &)`.
- Reuse the existing internal details apply logic used by create plan composition.
- Replace only the 7046 Local details field block.
- Keep normalization, arcade slot normalization, details push, and logging in the handler.
- Add focused tests for representative options and custom game fields.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
