# gc-template-only-duplicate-inventory-guard requirements

Status: Completed

## Problem

`audit_template_only_inventory_guard()` compares template-only switch emsgs with `MESSAGE_ROUTING_INVENTORY.md`, but duplicate documented `TEMPLATE_ONLY` emsgs can be hidden by set normalization.

## Requirements

- The audit shall report duplicate `TEMPLATE_ONLY` emsgs in `MESSAGE_ROUTING_INVENTORY.md` section `## 4. template_replay`.
- The duplicate check shall catch duplicates inside comma-separated cells and across multiple rows.
- Existing switch-vs-inventory drift checks shall remain unchanged.
- Production C++ template replay behavior shall remain unchanged.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
