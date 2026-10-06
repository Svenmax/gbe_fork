# gc-audit-replay-label-helper requirements

## Scope

Extract audit-only replay fixture label parsing into one named Python helper.

## Requirements

- The helper shall parse fixture text line by line with the existing `split(maxsplit=2)` semantics.
- The helper shall return the third field from each line that has exactly three fields.
- The helper shall ignore blank or malformed lines.
- Post-login dispatch audit shall use the helper while preserving existing fixture diagnostics.
- Production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change replay fixture file formats.
- Do not change routing truth tables.
- Do not broaden label parsing beyond the existing three-field line semantics.
