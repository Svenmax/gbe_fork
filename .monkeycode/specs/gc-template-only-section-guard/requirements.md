# gc-template-only-section-guard requirements

Status: Completed

## Problem

`audit_template_only_inventory_guard()` scans all of `MESSAGE_ROUTING_INVENTORY.md` for `TEMPLATE_ONLY` rows, so rows outside the template replay truth-table section can satisfy or perturb the audit.

## Requirements

- The audit shall read `TEMPLATE_ONLY` inventory rows only from `MESSAGE_ROUTING_INVENTORY.md` section `## 4. template_replay`.
- Existing template switch parsing and duplicate emsg detection shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that moving a `TEMPLATE_ONLY` inventory row outside §4 fails the audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
