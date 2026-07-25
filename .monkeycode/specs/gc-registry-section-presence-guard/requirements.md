# gc-registry-section-presence-guard requirements

Status: Completed

## Problem

`audit_registry_inventory_guard()` scopes registry inventory parsing to `MESSAGE_ROUTING_INVENTORY.md` §1, but if the section heading is renamed or removed the audit falls back to scanning the full routing inventory.

## Requirements

- The audit shall fail when `MESSAGE_ROUTING_INVENTORY.md` section `## 1. Registry` is missing.
- Existing registry emsg, metadata, and duplicate row checks shall remain unchanged when §1 exists.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that removing the §1 heading fails the registry inventory audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
