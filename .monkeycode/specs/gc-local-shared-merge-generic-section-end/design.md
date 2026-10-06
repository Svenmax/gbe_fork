# gc-local-shared-merge-generic-section-end design

Status: Completed

## Scope

This slice tightens only the Local/shared merge inventory section end boundary. It does not alter production code or documented inventory contents.

## Approach

- After locating `## 5. Local/shared merge inventory`, search for the next level-2 heading in the section tail.
- Slice §5 at that next heading instead of depending on `## 6.`.
- Add a regression test where the following heading is renumbered and a table row after it must be ignored.

## Safety

- Production C++ files remain untouched.
- Existing Local/shared merge checks remain active.
- Full GC verification remains the completion gate.
