# gc-inventory-duplicate-issues-helper design

Status: Completed

## Scope

This slice adds a Python helper for audit-side duplicate issue emission. It does not alter inventory truth tables or production C++ code.

## Approach

- Add `append_duplicate_issues(issues, duplicates, prefix)` near the inventory parsing helpers.
- Replace repeated duplicate issue loops in routing inventory audits with the helper.
- Add focused helper regression tests for numeric ordering and empty input.

## Safety

- Duplicate detection remains owned by `record_duplicate(...)`.
- Message prefixes remain explicit at each audit call site.
- Full GC verification remains the completion gate.
