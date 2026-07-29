# gc-registry-defensive-section-presence-guard design

Status: Completed

## Scope

This slice tightens only the registry-defensive template inventory audit's section boundary. It does not alter production routing or template replay behavior.

## Approach

- Detect the `## 4. template_replay` heading before parsing `REGISTRY_DEFENSIVE` inventory rows.
- Emit a focused diagnostic when the heading is missing.
- Preserve all existing helper, switch, duplicate, and mismatch diagnostics when the heading exists.

## Safety

- Production C++ files remain untouched.
- Existing registry-defensive emsg set comparison remains active.
- Full GC verification remains the completion gate.
