# gc-local-shared-sourcetv-metadata-apply design

## Current State

4508 game server info directly writes `GBE_local_lobby.tv_secret_code` and `GBE_local_lobby.tv_port`, then publishes practice lobby metadata.

## Design

- Add `apply_source_tv_metadata(GBE_LocalLobby &, std::uint64_t, std::uint32_t)` to `gbe_dota_lobby_state`.
- Preserve current semantics: each field updates only when the incoming value is non-zero.
- Return true when an applied non-zero value changes the stored value.
- Replace the direct 4508 field writes with the helper while keeping the active/lobby guard and unconditional metadata publish inside that guard.
- Add focused lobby state tests for applying values, preserving zero inputs, and reporting no-op updates.

## Validation

- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
