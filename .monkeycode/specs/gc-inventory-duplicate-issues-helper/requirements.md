# gc-inventory-duplicate-issues-helper requirements

Status: Completed

## Problem

Routing inventory audits repeat the same duplicate issue emission pattern: sort duplicate emsg values numerically and append a prefix plus the emsg. This is a small drift point after duplicate tracking was centralized.

## Requirements

- Routing inventory audits shall use one shared helper for duplicate issue emission.
- The helper shall preserve current numeric sort order for emsg values.
- Existing duplicate failure message text shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Helper-level tests shall cover numeric sorting and empty duplicate sets.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
