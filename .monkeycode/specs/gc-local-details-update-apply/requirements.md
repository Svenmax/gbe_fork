# gc-local-details-update-apply requirements

## Scope

Reduce Local lobby write scatter by moving 7046 set details options/custom_game writes behind a named state helper.

## Requirements

- 7046 set details shall apply options and custom_game fields through a `gbe::dota_lobby_state` helper.
- The helper shall preserve request `has_*` semantics.
- Existing lobby id mismatch logging, custom game normalization, arcade slot normalization, details push, and diagnostics order shall remain unchanged.

## Non-Goals

- Do not change 7046 parsing.
- Do not change custom game normalization behavior.
- Do not change details update push behavior.
