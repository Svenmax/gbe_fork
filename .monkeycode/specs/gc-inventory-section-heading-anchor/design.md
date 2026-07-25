# gc-inventory-section-heading-anchor design

Status: Completed

## Scope

This slice tightens only the shared Python audit helper `inventory_section(...)`. It does not alter production C++ code or inventory documents.

## Approach

- Replace substring `find()` heading lookup with a multiline regex anchored at line start.
- Keep the existing next level-2 heading end boundary.
- Add a focused helper regression test with inline heading text before the real heading.

## Safety

- Consumer-specific missing section messages remain at call sites.
- Existing section-end regression tests continue to cover each inventory consumer.
- Full GC verification remains the completion gate.
