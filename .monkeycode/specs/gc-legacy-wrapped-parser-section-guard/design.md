# gc-legacy-wrapped-parser-section-guard design

Status: Completed

## Scope

This slice tightens only the legacy wrapped parser documentation audit. It does not alter production routing, parser implementation, or handler behavior.

## Approach

- Slice `MESSAGE_ROUTING_INVENTORY.md` to section `## 5. 解析层职责` before looking for the `LEGACY_UNUSED` marker.
- Preserve the existing diagnostic: `MESSAGE_ROUTING must document GBE_ExtractWrappedDotaDirectContext as LEGACY_UNUSED`.
- Add a focused regression test that removes the §5 marker and appends the same marker outside §5.

## Safety

- Production C++ files remain untouched.
- Existing production call-site ban remains active across `GC_TUS`.
- Full GC verification remains the completion gate.
