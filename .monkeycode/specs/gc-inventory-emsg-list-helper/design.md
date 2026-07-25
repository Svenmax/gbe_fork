# gc-inventory-emsg-list-helper design

Status: In Progress

## Scope

This slice changes only Python audit diagnostics and iteration order for routing inventory emsg string sets. It does not alter inventory truth tables or production C++ code.

## Approach

- Replace routing inventory `sorted(<emsg_set>)` display calls with `sorted_numeric_values(<emsg_set>)`.
- Replace remaining routing inventory expected-emsg iteration with `sorted_numeric_values(...)`.
- Leave non-emsg generic sorting untouched.

## Safety

- Set comparisons remain unchanged.
- Existing issue prefixes remain at call sites.
- Full GC verification remains the completion gate.
