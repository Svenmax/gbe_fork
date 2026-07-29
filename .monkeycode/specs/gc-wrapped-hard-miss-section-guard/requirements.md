# gc-wrapped-hard-miss-section-guard requirements

Status: Completed

## Problem

`audit_wrapped_hard_miss_routing()` accepts any `HARD_MISS` line mentioning the wrapped hard miss helper anywhere in `MESSAGE_ROUTING_INVENTORY.md`, so a marker outside the fallback truth-table section can satisfy the audit.

## Requirements

- The audit shall require the wrapped `HARD_MISS` marker in `MESSAGE_ROUTING_INVENTORY.md` section `## 2. 仍留在 if/fallback 的路径（有意保留）`.
- The existing helper implementation and request-path checks shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that moving the marker outside §2 fails the audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
