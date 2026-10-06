# gc-registry-defensive-duplicate-inventory-guard design

Status: Completed

## Scope

This slice tightens only the `REGISTRY_DEFENSIVE` inventory audit. It does not alter production dispatch, template replay handling, direct conditional fallback, or wrapped hard-miss behavior.

## Approach

- Track seen `REGISTRY_DEFENSIVE` emsgs while parsing routing inventory rows.
- Emit a stable diagnostic for repeats: `MESSAGE_ROUTING registry-defensive inventory duplicates <emsg>`.
- Add a focused regression test that duplicates the `7091` `REGISTRY_DEFENSIVE` row and asserts the diagnostic.

## Safety

- Production C++ files remain untouched.
- Existing helper and switch drift checks remain active.
- Full GC verification remains the completion gate.
