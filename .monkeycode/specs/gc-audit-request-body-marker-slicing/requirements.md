# gc-audit-request-body-marker-slicing requirements

## Scope

Reuse the shared audit-only marker slicing helper for direct and wrapped post-login request body extraction.

## Requirements

- Direct post-login request body extraction shall use `text_between_markers(...)`.
- Wrapped post-login request body extraction shall use `text_between_markers(...)`.
- Missing marker behavior shall remain fail-closed via an empty body string.
- Existing routing audit issue strings and production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change routing truth tables.
- Do not change production C++ request routing.
- Do not introduce new helper semantics beyond the existing `text_between_markers(...)` contract.
