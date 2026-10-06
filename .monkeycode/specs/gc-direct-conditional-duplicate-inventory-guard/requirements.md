# gc-direct-conditional-duplicate-inventory-guard requirements

Status: Completed

## Problem

`audit_direct_conditional_fallback_routing()` compares documented `CONDITIONAL_*` emsgs with the expected set, but duplicate documented rows can be hidden by set normalization.

## Requirements

- The audit shall report duplicate direct conditional fallback emsgs in `MESSAGE_ROUTING_INVENTORY.md` section `## 2. 仍留在 if/fallback 的路径（有意保留）`.
- Existing helper emsg, call-order, inline-check, and inventory set checks shall remain unchanged.
- Production C++ direct fallback behavior shall remain unchanged.
- A regression test shall prove that duplicating an existing conditional row fails the audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
