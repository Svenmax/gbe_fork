# gc-registry-generic-section-end design

Status: Completed

## Scope

This slice tightens only the registry inventory section end boundary. It does not alter production registry routing or documented inventory contents.

## Approach

- After locating `## 1. Registry`, search for the next level-2 heading in the section tail.
- Slice §1 at that next heading instead of depending on `---`.
- Add a regression test where the separator after §1 is missing and §2 rows must be ignored.

## Safety

- Production C++ files remain untouched.
- Existing registry inventory checks remain active.
- Full GC verification remains the completion gate.
