# gc-registry-duplicate-inventory-guard requirements

Status: Completed

## Problem

`audit_registry_inventory_guard()` compares production registry emsgs with `MESSAGE_ROUTING_INVENTORY.md`, but duplicate documented registry rows can be hidden by set/dict normalization.

## Requirements

- The audit shall report duplicate emsg rows in `MESSAGE_ROUTING_INVENTORY.md` section `## 1. Registry`.
- The audit shall keep the existing emsg-set and metadata drift checks unchanged.
- The change shall not modify production C++ routing behavior.
- A regression test shall prove that duplicating an existing registry row fails the audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
