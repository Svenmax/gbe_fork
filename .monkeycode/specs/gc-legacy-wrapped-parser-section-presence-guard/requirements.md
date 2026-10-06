# gc-legacy-wrapped-parser-section-presence-guard requirements

Status: Completed

## Problem

`audit_legacy_wrapped_parser_guard()` scopes the `LEGACY_UNUSED` marker to `MESSAGE_ROUTING_INVENTORY.md` §5, but if the section heading is renamed or removed the audit falls back to scanning the full routing inventory.

## Requirements

- The audit shall fail when `MESSAGE_ROUTING_INVENTORY.md` section `## 5. 解析层职责` is missing.
- Existing `LEGACY_UNUSED` marker and production call checks shall remain unchanged when §5 exists.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that removing the §5 heading fails the legacy wrapped parser audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
