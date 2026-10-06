# gc-inventory-emsg-diff-helper requirements

Status: In Progress

## Problem

Routing inventory audits repeat the same emsg set-diff diagnostic pattern at several boundaries. Each call site already uses numeric ordering, but the formatting remains duplicated and can drift.

## Requirements

- A shared audit helper shall append emsg set-diff diagnostics using numeric emsg ordering.
- Existing issue prefixes and expected labels shall remain unchanged.
- Existing set comparison semantics shall remain unchanged.
- Production C++ behavior shall remain unchanged.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
