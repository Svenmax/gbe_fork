# gc-inventory-multi-numeric-cell-helper design

## Current State

`numeric_markdown_cell(...)` covers single-emsg cells. Template-only inventory rows can contain multiple emsg values in the first cell and currently parse them inline with `re.findall(r'\b\d+\b', cells[0])`.

## Design

- Add `numeric_markdown_cell_values(cell)` next to the Markdown inventory helpers.
- Implement it as a thin wrapper around the existing numeric token pattern used by template-only inventory parsing.
- Replace the template-only inline regex loop with the helper.
- Add focused helper tests for multiple values, no values, and order preservation.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
