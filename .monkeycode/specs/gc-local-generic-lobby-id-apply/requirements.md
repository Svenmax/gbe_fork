# gc-local-generic-lobby-id-apply requirements

## Scope

Reduce Local lobby write scatter by moving generic_lobby_id writes behind a named state helper.

## Requirements

- 7038 generic lobby create shall apply generic_lobby_id through a `gbe::dota_lobby_state` helper.
- 7040 leave fallback generic lookup shall apply generic_lobby_id through the same helper.
- Leave-generic cleanup shall clear generic_lobby_id through the same helper.
- Shared restore shall reuse the same helper semantics.
- Existing create action, settings sync, leave cleanup, and logging order shall remain unchanged.

## Non-Goals

- Do not change generic lobby lookup behavior.
- Do not change create action sequencing.
- Do not change settings sync or leave lobby behavior.
