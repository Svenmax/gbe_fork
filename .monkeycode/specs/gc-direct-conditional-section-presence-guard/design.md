# gc-direct-conditional-section-presence-guard design

Status: Completed

## Scope

This slice tightens only the direct conditional fallback inventory audit's section boundary. It does not alter production direct fallback routing or template replay behavior.

## Approach

- Detect the `## 2. 仍留在 if/fallback 的路径` heading before parsing `CONDITIONAL_*` inventory rows.
- Emit a focused diagnostic when the heading is missing.
- Preserve all existing helper, route-order, duplicate, and mismatch diagnostics when the heading exists.

## Safety

- Production C++ files remain untouched.
- Existing direct conditional emsg set comparison remains active.
- Full GC verification remains the completion gate.
