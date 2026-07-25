# gc-local-shared-owner-team-slot-apply requirements

## Scope

Reduce Local/shared dual-track risk by moving the 7034 draft owner team/slot Local writes behind named state helpers.

## Requirements

- Owner team and owner slot shall be applied to `GBE_LocalLobby` through `gbe::dota_lobby_state` helpers.
- Matching owner team and owner slot inputs shall be reported as unchanged.
- 7034 draft owner update shall keep existing non-zero slot guard behavior.
- Shared-to-local owner team/slot restore behavior shall remain unchanged.

## Non-Goals

- Do not change 7034 parsing or response payload behavior.
- Do not change owner hero or member slot ownership.
- Do not move shared Store ownership.
