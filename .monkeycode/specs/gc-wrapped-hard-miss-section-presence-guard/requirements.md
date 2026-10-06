# gc-wrapped-hard-miss-section-presence-guard requirements

Status: Completed

## Problem

`audit_wrapped_hard_miss_routing()` scopes `HARD_MISS` inventory parsing to fallback §2, but if the section heading is renamed or removed the audit falls back to scanning the full routing inventory.

## Requirements

- The audit shall fail when `MESSAGE_ROUTING_INVENTORY.md` section `## 2. 仍留在 if/fallback 的路径` is missing.
- Existing helper implementation, request-path, and hard-miss marker checks shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that removing the §2 heading fails the wrapped hard miss audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
