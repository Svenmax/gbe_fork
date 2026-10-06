# gc-audit-any-token-helper requirements

## Scope

Extract repeated audit-only any-token membership checks into one shared Python helper.

## Requirements

- The helper shall return true when any token in the provided token collection appears in the text.
- The helper shall return false when the token collection is empty or no token appears in the text.
- Wrapped hard-miss routing audit shall use the helper for prohibited route token checks.
- Existing issue strings and production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change routing truth tables.
- Do not change production C++ routing behavior.
- Do not introduce word-boundary matching; preserve existing substring membership semantics.
