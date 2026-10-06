# gc-registry-defensive-section-presence-guard requirements

Status: Completed

## Problem

`audit_registry_defensive_template_routing()` scopes `REGISTRY_DEFENSIVE` parsing to template replay §4, but if the section heading is renamed or removed the audit falls back to scanning the full routing inventory.

## Requirements

- The audit shall fail when `MESSAGE_ROUTING_INVENTORY.md` section `## 4. template_replay` is missing.
- Existing helper parsing, switch exclusion, expected emsg comparison, and duplicate emsg detection shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that removing the §4 heading fails the registry-defensive audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
