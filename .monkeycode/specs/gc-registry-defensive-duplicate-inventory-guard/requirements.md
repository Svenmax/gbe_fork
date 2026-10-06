# gc-registry-defensive-duplicate-inventory-guard requirements

Status: Completed

## Problem

`audit_registry_defensive_template_routing()` compares documented `REGISTRY_DEFENSIVE` emsgs with the expected set, but duplicate documented rows can be hidden by set normalization.

## Requirements

- The audit shall report duplicate `REGISTRY_DEFENSIVE` emsg rows in `MESSAGE_ROUTING_INVENTORY.md` section `## 4. template_replay`.
- Existing helper emsg, switch exclusion, and inventory set checks shall remain unchanged.
- Production C++ template replay and registry behavior shall remain unchanged.
- A regression test shall prove that duplicating an existing `REGISTRY_DEFENSIVE` row fails the audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
