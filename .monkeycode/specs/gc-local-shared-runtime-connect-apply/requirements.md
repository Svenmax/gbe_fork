# gc-local-shared-runtime-connect-apply requirements

## Scope

Reduce Local/shared dual-track risk by moving the 4508 runtime connect Local write behind a named state helper.

## Requirements

- Runtime connect shall be applied to `GBE_LocalLobby` through a `gbe::dota_lobby_state` helper.
- Empty runtime connect input shall preserve the existing lobby connect value.
- Matching runtime connect input shall be reported as unchanged.
- 4508 post-login handling shall keep the existing LAN preserve guard.
- Shared Store compare_update behavior and generation gating shall remain unchanged.

## Non-Goals

- Do not change connect normalization.
- Do not move shared Store update ownership.
- Do not change SourceTV metadata handling.
