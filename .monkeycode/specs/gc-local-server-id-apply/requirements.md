# gc-local-server-id-apply requirements

## Scope

Reduce Local lobby write scatter by moving recover coordinator derived server_id writes behind a named state helper.

## Requirements

- Recover coordinator shall apply derived server_id through a `gbe::dota_lobby_state` helper.
- Runtime metadata helper shall reuse the same server_id apply semantics.
- Zero server_id input shall be applied exactly when the caller has passed its own guard.
- Existing derived server guard, shared Store compare_update, and diagnostics order shall remain unchanged.

## Non-Goals

- Do not change server id derivation.
- Do not change recover coordinator guard conditions.
- Do not change shared Store update behavior.
