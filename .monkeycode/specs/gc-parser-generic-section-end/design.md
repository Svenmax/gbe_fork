# gc-parser-generic-section-end design

Status: Completed

## Scope

This slice tightens only the `MESSAGE_ROUTING_INVENTORY.md` §5 section end boundary for the legacy wrapped parser audit. It does not alter production routing or parser code.

## Approach

- After locating `## 5. 解析层职责`, search for the next level-2 heading in the section tail.
- Slice §5 at that next heading for the legacy wrapped parser audit consumer.
- Add a focused regression test for missing separator after §5 with an out-of-section `LEGACY_UNUSED` marker.

## Safety

- Production C++ files remain untouched.
- Existing legacy wrapped parser checks remain active.
- Full GC verification remains the completion gate.
