# gc-inventory-numeric-cell-helper requirements

Status: In Progress

## Problem

Routing inventory audits extract numeric emsg values from Markdown table cells with repeated regex snippets. Exact single-emsg cells and descriptive cells with one emsg use slightly different matching rules that can drift.

## Requirements

- A shared audit helper shall extract a numeric emsg from a Markdown table cell.
- The helper shall support exact-cell matching and first-number matching.
- Existing inventory parsing semantics shall remain unchanged.
- Production C++ behavior shall remain unchanged.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
