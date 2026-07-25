# gc-template-only-section-guard design

Status: Completed

## Scope

This slice tightens only the template-only inventory audit. It does not alter production template replay routing, registry dispatch, or direct/wrapped fallback behavior.

## Approach

- Slice `MESSAGE_ROUTING_INVENTORY.md` to section `## 4. template_replay` before collecting `TEMPLATE_ONLY` rows.
- Keep the existing expected emsg set and duplicate row diagnostics.
- Add a focused regression test that removes a §4 `TEMPLATE_ONLY` row and appends the same row outside §4.

## Safety

- Production C++ files remain untouched.
- Existing switch-vs-inventory comparison remains active.
- Full GC verification remains the completion gate.
