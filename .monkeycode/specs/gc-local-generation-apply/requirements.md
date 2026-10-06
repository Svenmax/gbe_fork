# gc-local-generation-apply requirements

## Scope

Reduce Local lobby write scatter by moving generation writes behind a named state helper.

## Requirements

- Create, join, runtime reset, lifecycle clear, and recover paths shall apply Local generation through a `gbe::dota_lobby_state` helper.
- Shared restore shall reuse the same helper semantics.
- Existing generation advance, counter update, runtime clear, and action sequencing shall remain unchanged.

## Non-Goals

- Do not change generation counter logic.
- Do not change generation boundary decisions.
- Do not change runtime reset or recover behavior.
