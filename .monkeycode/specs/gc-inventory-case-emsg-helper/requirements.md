# gc-inventory-case-emsg-helper requirements

Status: In Progress

## Problem

Template replay routing audits parse `case ...:` labels in multiple places. Numeric labels and named Dota constants are converted to emsg strings with repeated local logic, which can drift as routing guard coverage grows.

## Requirements

- A shared audit helper shall extract emsg strings from C++ `case` labels.
- The helper shall map known named Dota constants to numeric emsg strings.
- Existing routing comparison semantics shall remain unchanged.
- Production C++ behavior shall remain unchanged.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
