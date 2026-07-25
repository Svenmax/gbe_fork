# gc-registry-duplicate-inventory-guard design

Status: Completed

## Scope

This slice tightens the registry inventory audit only. It does not change registry dispatch, handlers, request modes, lifecycle classes, or template fallback behavior.

## Approach

- Track seen inventory emsg numbers while parsing `## 1. Registry`.
- Record any repeated emsg before the existing set comparison collapses the row.
- Emit a stable diagnostic: `MESSAGE_ROUTING registry inventory duplicates <emsg>`.
- Add a focused regression test that duplicates the `7091` registry row and asserts the diagnostic.

## Safety

- Production C++ files remain untouched.
- Existing metadata checks still compare `HandlerId`, mode, and lifecycle against production `kTable`.
- Full GC verification remains the completion gate.
