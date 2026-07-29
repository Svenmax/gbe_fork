# gc-local-broadcast-channel-apply requirements

## Scope

Reduce Local lobby write scatter by moving broadcast channel join, update, and close field writes behind named state helpers.

## Requirements

- 7149 join shall apply the complete broadcast channel field group through a `gbe::dota_lobby_state` helper.
- 7367 update shall patch only present optional broadcast metadata fields through a `gbe::dota_lobby_state` helper.
- 8054 close shall clear broadcast metadata while preserving the request channel id through a `gbe::dota_lobby_state` helper.
- Existing publish, details update, ack, and log ordering shall remain unchanged.

## Non-Goals

- Do not change broadcast payload parsing.
- Do not change practice lobby details payload behavior.
- Do not move shared Store ownership.
