# gc-inventory-sorted-numeric-helper design

Status: Completed

## Scope

This slice adds a Python helper for audit-side numeric sorting. It does not alter inventory truth tables or production C++ code.

## Approach

- Add `sorted_numeric_values(values)` near inventory parsing helpers.
- Use it from duplicate issue emission and registry metadata comparison iteration.
- Add focused helper regression tests for numeric sorting of string values.

## Safety

- Duplicate issue message prefixes and formatting remain unchanged.
- Existing inventory guard tests continue to cover each consumer.
- Full GC verification remains the completion gate.
