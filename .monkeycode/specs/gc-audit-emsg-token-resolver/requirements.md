# gc-audit-emsg-token-resolver requirements

## Scope

Extract audit-only registry emsg token resolution into one shared Python helper.

## Requirements

- The helper shall resolve decimal tokens with an optional `u` suffix to the decimal string without suffix.
- The helper shall resolve known `GBE_k...` tokens using the provided constants map.
- The helper shall return `None` for unknown non-literal tokens.
- Registry inventory audit shall use the helper while preserving existing issue strings.
- Production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change routing truth tables.
- Do not change production registry entries.
- Do not broaden token formats beyond existing numeric literal and `GBE_k...` constant semantics.
