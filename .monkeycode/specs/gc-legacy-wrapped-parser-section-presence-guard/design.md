# gc-legacy-wrapped-parser-section-presence-guard design

Status: Completed

## Scope

This slice tightens only the legacy wrapped parser audit's inventory section boundary. It does not alter production parser or routing behavior.

## Approach

- Detect the `## 5. 解析层职责` heading before checking the `LEGACY_UNUSED` marker.
- Emit a focused diagnostic when the heading is missing.
- Preserve existing marker and production-call diagnostics when the heading exists.

## Safety

- Production C++ files remain untouched.
- Existing legacy parser usage checks remain active.
- Full GC verification remains the completion gate.
