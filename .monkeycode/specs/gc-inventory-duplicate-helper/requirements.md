# gc-inventory-duplicate-helper requirements

Status: Completed

## Problem

Inventory audits repeat the same duplicate tracking pattern with separate seen and duplicate sets. This makes duplicate behavior harder to keep aligned across routing inventory guards.

## Requirements

- Inventory audits shall use one shared helper for recording duplicate inventory values.
- The helper shall preserve current behavior: first occurrence is recorded as seen, repeated occurrences are recorded as duplicates.
- Existing duplicate failure messages shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Helper-level tests shall cover first occurrence and repeated occurrence behavior.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
