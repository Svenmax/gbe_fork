# gc-fallback-generic-section-end design

Status: Completed

## Scope

This slice tightens only the `MESSAGE_ROUTING_INVENTORY.md` §2 section end boundary for fallback inventory audits. It does not alter production direct or wrapped routing.

## Approach

- After locating `## 2. 仍留在 if/fallback 的路径`, search for the next level-2 heading in the section tail.
- Slice §2 at that next heading for both direct conditional fallback and wrapped hard miss audit consumers.
- Add focused regression tests for missing separator after §2 with out-of-section rows.

## Safety

- Production C++ files remain untouched.
- Existing direct conditional and wrapped hard miss checks remain active.
- Full GC verification remains the completion gate.
