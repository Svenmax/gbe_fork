# gc-template-only-section-presence-guard design

Status: Completed

## Scope

This slice tightens only the template-only inventory audit's section boundary. It does not alter production routing or the template replay switch.

## Approach

- Detect the `## 4. template_replay` heading before parsing `TEMPLATE_ONLY` inventory rows.
- Emit a focused diagnostic when the heading is missing.
- Preserve all existing row parsing and mismatch diagnostics when the heading exists.

## Safety

- Production C++ files remain untouched.
- Existing template-only emsg set comparison remains active.
- Full GC verification remains the completion gate.
