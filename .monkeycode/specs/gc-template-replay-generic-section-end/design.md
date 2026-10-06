# gc-template-replay-generic-section-end design

Status: Completed

## Scope

This slice tightens only the `MESSAGE_ROUTING_INVENTORY.md` §4 section end boundary for template replay inventory audits. It does not alter production template replay routing.

## Approach

- After locating `## 4. template_replay`, search for the next level-2 heading in the section tail.
- Slice §4 at that next heading for both template-only and registry-defensive audit consumers.
- Add focused regression tests for missing separator after §4 with out-of-section rows.

## Safety

- Production C++ files remain untouched.
- Existing template-only and registry-defensive checks remain active.
- Full GC verification remains the completion gate.
