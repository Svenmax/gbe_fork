# gc-local-custom-game-loading-metadata-apply requirements

## Scope

Reduce Local lobby write scatter by moving 8052 started loading custom game metadata writes behind a named state helper.

## Requirements

- 8052 started loading shall apply non-zero custom game id through a `gbe::dota_lobby_state` helper.
- 8052 started loading shall apply non-zero game start time through the same helper.
- Zero custom game id and zero start time shall preserve existing Local values.
- Existing launch setup calculation and lifecycle decision ordering shall remain unchanged.

## Non-Goals

- Do not change 8052 request parsing.
- Do not change lifecycle state machine decisions.
- Do not change response or publish behavior.
