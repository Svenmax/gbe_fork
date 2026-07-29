# gc-local-kick-member-snapshot-apply requirements

## Scope

Reduce Local object replacement scatter by moving 7081 kick-member success snapshot application behind a named state helper.

## Requirements

- 7081 kick-member success shall write the prepared Local snapshot through a `gbe::dota_lobby_state` helper.
- Existing generic kick call, shared publish, details update, and diagnostics order shall remain unchanged.
- Kick failure behavior shall remain unchanged.

## Non-Goals

- Do not change generic lobby kick behavior.
- Do not change member removal planning.
- Do not change 7081 response or details update behavior.
