# gc-inventory-section-heading-constants design

Status: In Progress

## Scope

This slice changes only Python audit constants and section lookup call sites. It does not alter Markdown truth tables, parsing rules, or production C++ code.

## Approach

- Add named constants for routing registry, fallback, template replay, parser, and Local/shared merge inventory headings.
- Replace direct heading string literals in `inventory_section(...)` calls with those constants.
- Leave all issue strings and audit control flow unchanged.

## Safety

- Existing tests cover missing section fail-fast behavior.
- Full GC verification remains the completion gate.
