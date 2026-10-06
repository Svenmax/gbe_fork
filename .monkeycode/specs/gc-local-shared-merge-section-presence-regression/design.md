# gc-local-shared-merge-section-presence-regression design

Status: Completed

## Scope

This slice adds regression coverage for the existing Local/shared merge inventory section-presence guard. It does not alter production code or the audit implementation.

## Approach

- Rename `## 5. Local/shared merge inventory` in a test copy of `LOCAL_LOBBY_USAGE.md`.
- Assert that `audit_local_shared_merge_inventory()` emits `LOCAL_LOBBY merge inventory section not found`.
- Preserve existing metadata, duplicate, and definition checks.

## Safety

- Production C++ files remain untouched.
- Audit implementation remains unchanged.
- Full GC verification remains the completion gate.
