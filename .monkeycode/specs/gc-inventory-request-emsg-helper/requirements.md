# gc-inventory-request-emsg-helper requirements

Status: In Progress

## Problem

Direct conditional fallback routing audit extracts emsgs from `request_emsg == ...` comparisons with local regex and ad hoc named-token checks. This can drift from other emsg extraction helpers.

## Requirements

- A shared audit helper shall extract emsg strings from C++ `request_emsg == ...` comparisons.
- The helper shall map known named request tokens to numeric emsg strings.
- Existing direct conditional fallback comparison semantics shall remain unchanged.
- Production C++ behavior shall remain unchanged.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
