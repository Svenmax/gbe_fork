# gc-local-shared-owner-connected-apply requirements

## Scope

Reduce Local/shared dual-track risk by moving the launch coordinator owner-connected Local write behind a named state helper.

## Requirements

- Owner connected state shall be applied to `GBE_LocalLobby` through a `gbe::dota_lobby_state` helper.
- Matching owner connected input shall be reported as unchanged.
- `GBE_SetDotaLobbyMemberConnected` shall keep its existing member update and changed aggregation semantics.
- Shared-to-local owner connected restore behavior shall remain unchanged.

## Non-Goals

- Do not change connection lifecycle publish behavior.
- Do not change member connected merge behavior.
- Do not move shared Store ownership.
