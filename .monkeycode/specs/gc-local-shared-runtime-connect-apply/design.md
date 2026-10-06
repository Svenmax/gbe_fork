# gc-local-shared-runtime-connect-apply design

## Current State

4508 game server info computes `runtime_connect`, checks the LAN preserve guard, writes `GBE_local_lobby.connect`, then generation-gated updates shared Store connect.

## Design

- Add `apply_runtime_connect(GBE_LocalLobby &, const std::string &)` to `gbe_dota_lobby_state`.
- Preserve current semantics: empty input and matching input do not mutate Local.
- Return true only when Local connect changes.
- Keep the 4508 active/lobby guard, LAN preserve guard, previous-connect logging, and shared Store compare_update in the handler.
- Add focused lobby state tests for empty input preservation, changed input application, and matching input no-op reporting.

## Validation

- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
