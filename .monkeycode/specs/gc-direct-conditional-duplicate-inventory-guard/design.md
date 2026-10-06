# gc-direct-conditional-duplicate-inventory-guard design

Status: Completed

## Scope

This slice tightens only the direct conditional fallback inventory audit. It does not alter direct post-login dispatch, template replay routing, registry-defensive routing, or wrapped hard-miss behavior.

## Approach

- Restrict conditional inventory parsing to the `## 2. 仍留在 if/fallback 的路径（有意保留）` routing section.
- Track seen `CONDITIONAL_*` emsgs while parsing rows.
- Emit a stable diagnostic for repeats: `MESSAGE_ROUTING direct conditional fallback inventory duplicates <emsg>`.
- Add a focused regression test that duplicates the `AuthList (5432)` row and asserts the diagnostic.

## Safety

- Production C++ files remain untouched.
- Existing helper, order, inline fallback, and inventory drift checks remain active.
- Full GC verification remains the completion gate.
