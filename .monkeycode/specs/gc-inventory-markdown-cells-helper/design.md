# gc-inventory-markdown-cells-helper design

Status: Completed

## Scope

This slice adds a Python helper for audit-side Markdown table row parsing. It does not alter inventory truth tables or production C++ code.

## Approach

- Add `markdown_cells(line)` near `inventory_section(...)`.
- Replace repeated `[cell.strip() for cell in line.strip().strip("|").split("|")]` expressions in inventory audits with the helper.
- Add focused helper regression tests for trimmed cells and rows without outer pipes.

## Safety

- The helper intentionally preserves existing simple split semantics.
- Existing inventory guard tests continue to exercise each consumer.
- Full GC verification remains the completion gate.
