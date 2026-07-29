# gc-local-members-restore requirements

## Scope

Reduce shared-to-local runtime restore write scatter by moving Local members restore behind a named state helper.

## Requirements

- Shared-to-local runtime restore shall apply `members` through a `gbe::dota_lobby_state` helper.
- The helper shall report whether Local members changed.
- Existing changed aggregation, sync settings, rich presence replay, private lobby snapshot replay, and login sync order shall remain unchanged.

## Non-Goals

- Do not change member equality semantics.
- Do not change shared Store snapshot behavior.
- Do not change post-restore side effects.
