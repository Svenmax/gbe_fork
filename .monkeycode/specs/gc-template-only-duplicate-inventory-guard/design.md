# gc-template-only-duplicate-inventory-guard design

Status: Completed

## Scope

This slice tightens only the `TEMPLATE_ONLY` inventory audit. It does not change template replay routing, registry-defensive routing, direct conditional fallback, or wrapped hard-miss behavior.

## Approach

- Track seen `TEMPLATE_ONLY` emsgs while parsing routing inventory rows.
- Extract every numeric emsg from the first column, preserving existing comma-separated row support.
- Emit a stable diagnostic for repeats: `MESSAGE_ROUTING template-only inventory duplicates <emsg>`.
- Add a regression test that duplicates `8218` in the `TEMPLATE_ONLY` inventory and asserts the diagnostic.

## Safety

- Production C++ files remain untouched.
- Existing `TEMPLATE_ONLY` set comparison against the production switch remains active.
- Full GC verification remains the completion gate.
