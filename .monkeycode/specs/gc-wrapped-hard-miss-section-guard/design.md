# gc-wrapped-hard-miss-section-guard design

Status: Completed

## Scope

This slice tightens only the wrapped hard miss inventory audit. It does not alter production wrapped routing, direct fallback, registry dispatch, or template replay behavior.

## Approach

- Slice `MESSAGE_ROUTING_INVENTORY.md` to section `## 2. 仍留在 if/fallback 的路径（有意保留）` before looking for `HARD_MISS`.
- Preserve the existing diagnostic: `MESSAGE_ROUTING wrapped hard miss must mention wrapped hard miss helper`.
- Add a focused regression test that removes the §2 wrapped hard miss row and appends the same marker outside §2.

## Safety

- Production C++ files remain untouched.
- Existing helper body, call-order, and miss-routing checks remain active.
- Full GC verification remains the completion gate.
