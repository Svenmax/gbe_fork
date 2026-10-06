# gc-direct-conditional-section-presence-guard requirements

Status: Completed

## Problem

`audit_direct_conditional_fallback_routing()` scopes `CONDITIONAL_*` inventory parsing to fallback §2, but if the section heading is renamed or removed the audit falls back to scanning the full routing inventory.

## Requirements

- The audit shall fail when `MESSAGE_ROUTING_INVENTORY.md` section `## 2. 仍留在 if/fallback 的路径` is missing.
- Existing helper parsing, request-path order checks, inline fallback checks, expected emsg comparison, and duplicate emsg detection shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that removing the §2 heading fails the direct conditional audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
