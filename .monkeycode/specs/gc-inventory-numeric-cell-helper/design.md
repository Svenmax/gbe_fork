# gc-inventory-numeric-cell-helper design

Status: In Progress

## Scope

This slice changes only Python audit helper code and tests. It does not change Markdown truth tables or production C++ code.

## Approach

- Add `numeric_markdown_cell(cell, exact=True)` near existing inventory helpers.
- Use exact matching for registry and registry-defensive inventory rows.
- Use first-number matching for direct conditional fallback rows.
- Leave template-only multi-emsg cell parsing unchanged.

## Safety

- Existing issue strings stay unchanged.
- Existing duplicate detection sets stay unchanged.
- Full GC verification remains the completion gate.
