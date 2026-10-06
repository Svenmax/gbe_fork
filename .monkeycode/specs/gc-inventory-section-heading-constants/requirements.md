# gc-inventory-section-heading-constants requirements

Status: In Progress

## Problem

Routing and Local/shared inventory audits repeat Markdown section heading strings at multiple lookup sites. The lookups already share section slicing behavior, but repeated literals can drift when headings change.

## Requirements

- Inventory section heading strings shall be centralized as named audit constants.
- Existing missing-section issue messages shall remain unchanged.
- Existing section slicing semantics shall remain unchanged.
- Production C++ behavior shall remain unchanged.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
