# gc-local-shared-merge-section-presence-regression requirements

Status: Completed

## Problem

`audit_local_shared_merge_inventory()` already fails when `LOCAL_LOBBY_USAGE.md` lacks the Local/shared merge inventory section, but that fail-fast behavior is not covered by a regression test.

## Requirements

- The existing audit diagnostic for missing `## 5. Local/shared merge inventory` shall remain covered by a regression test.
- Existing Local/shared merge entrypoint, owner, field group, duplicate, and definition checks shall remain unchanged.
- Production C++ behavior shall remain unchanged.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
