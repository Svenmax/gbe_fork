# gc-registry-section-presence-guard design

Status: Completed

## Scope

This slice tightens only the registry inventory audit's section boundary. It does not alter production registry routing or handler metadata.

## Approach

- Detect the `## 1. Registry` heading before parsing documented registry rows.
- Emit a focused diagnostic when the heading is missing.
- Preserve all existing registry emsg set, metadata, and duplicate row diagnostics when the heading exists.

## Safety

- Production C++ files remain untouched.
- Existing registry inventory checks remain active.
- Full GC verification remains the completion gate.
