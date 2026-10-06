# gc-inventory-emsg-diff-helper design

Status: In Progress

## Scope

This slice changes only Python audit diagnostic construction for routing emsg set mismatches. It does not change inventory parsing, truth tables, or production C++ code.

## Approach

- Add `append_emsg_set_diff_issue(issues, prefix, actual, expected, expected_label="expected")` near the existing inventory helpers.
- Keep call-site comparison conditions unchanged.
- Use the helper for routing registry, template replay, and direct conditional fallback mismatch diagnostics.

## Safety

- Existing message prefixes remain call-site owned.
- Numeric ordering remains delegated to `sorted_numeric_values(...)`.
- Existing tests and full GC verification remain the completion gate.
