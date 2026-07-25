# gc-inventory-section-exact-heading design

Status: Completed

## Scope

This slice tightens only the shared Python audit helper `inventory_section(...)`. It does not alter production C++ code or inventory truth tables.

## Approach

- Replace the line-start heading regex with a boundary-aware heading regex that allows line end or parenthetical annotations after the requested heading text.
- Keep the existing next level-2 heading end boundary.
- Add focused helper regression tests for a same-prefix longer heading and a parenthetical heading annotation.

## Safety

- Consumer-specific missing section messages remain at call sites.
- Existing line-start and next-heading tests remain in place.
- Full GC verification remains the completion gate.
