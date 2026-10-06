# gc-inventory-emsg-list-helper requirements

Status: In Progress

## Problem

Routing inventory diagnostics still print emsg string sets through plain `sorted(...)`, while duplicate diagnostics and registry metadata iteration use numeric emsg ordering. This can make diagnostic order drift for multi-digit emsg values.

## Requirements

- Routing inventory emsg set diagnostics shall use `sorted_numeric_values(...)` for display order.
- Routing inventory emsg iteration shall use the same numeric helper where values are emsg strings.
- Existing issue prefixes and comparison semantics shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Existing routing inventory tests shall continue to pass.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
