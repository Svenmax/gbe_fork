# gc-inventory-sorted-numeric-helper requirements

Status: Completed

## Problem

Inventory audits still hand-code numeric sorting for emsg-like string values in multiple places. This is a small drift point around duplicate messages and registry metadata comparison order.

## Requirements

- Inventory audits shall use one shared helper for numeric sorting of string values.
- The helper shall preserve current numeric sort behavior.
- Existing audit failure message text shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Helper-level tests shall cover numeric ordering for string values.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
