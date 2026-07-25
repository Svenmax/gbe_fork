# gc-audit-marker-order-helper requirements

## Scope

Extract repeated audit-only marker order comparisons into one shared Python helper.

## Requirements

- The helper shall return true only when both markers exist and the first marker appears after the reference marker.
- The helper shall return false when either marker is missing.
- Direct and wrapped routing audits shall use the helper for same-shape order checks.
- Existing issue strings and production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change routing truth tables.
- Do not change production C++ routing behavior.
- Do not introduce broader parsing beyond the existing marker-position comparison semantics.
