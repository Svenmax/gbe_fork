# gc-inventory-duplicate-helper design

Status: Completed

## Scope

This slice adds a Python helper for audit-side duplicate tracking. It does not alter inventory truth tables or production C++ code.

## Approach

- Add `record_duplicate(seen, duplicates, value)` near the inventory parsing helpers.
- Replace repeated seen/duplicate set updates in routing inventory audits with the helper.
- Add focused helper regression tests for first and repeated values.

## Safety

- Existing duplicate message formatting remains at each audit call site.
- Existing inventory guard tests continue to cover each consumer.
- Full GC verification remains the completion gate.
