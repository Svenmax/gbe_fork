# gc-local-runtime-metadata-apply requirements

## Scope

Reduce Local lobby write scatter by moving generic metadata publish runtime connect/server id writes behind a named state helper.

## Requirements

- Generic metadata publish shall apply Local connect/server id through a `gbe::dota_lobby_state` helper.
- Empty connect input shall be applied to Local connect exactly as published metadata.
- Zero server id input shall be applied to Local server id exactly as published metadata.
- Existing shared Store compare_update, generic lobby metadata writes, and diagnostic log order shall remain unchanged.

## Non-Goals

- Do not change generic metadata publish payload values.
- Do not change server id clear behavior for shared Store update.
- Do not change 4508 runtime connect behavior.
