# gc-wrapped-hard-miss-section-presence-guard design

Status: Completed

## Scope

This slice tightens only the wrapped hard miss inventory audit's section boundary. It does not alter production wrapped routing or hard-miss behavior.

## Approach

- Detect the `## 2. 仍留在 if/fallback 的路径` heading before checking the `HARD_MISS` inventory marker.
- Emit a focused diagnostic when the heading is missing.
- Preserve all existing helper body, request-path, and marker diagnostics when the heading exists.

## Safety

- Production C++ files remain untouched.
- Existing hard-miss behavior checks remain active.
- Full GC verification remains the completion gate.
