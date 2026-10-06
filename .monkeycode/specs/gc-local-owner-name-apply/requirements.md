# gc-local-owner-name-apply requirements

## Scope

Reduce Local owner metadata write scatter by moving generic lobby owner adoption `owner_name` writes behind a named state helper.

## Requirements

- Generic lobby owner adoption shall apply `owner_name` through a `gbe::dota_lobby_state` helper.
- The helper shall report whether the Local owner name changed.
- Existing owner adoption decisions, local owner publish, metadata publish, and diagnostics order shall remain unchanged.

## Non-Goals

- Do not change owner adoption policy.
- Do not change generic lobby owner lookup.
- Do not change owner member adoption behavior.
