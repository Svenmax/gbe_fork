# gc-audit-text-between-markers-helper requirements

## Scope

Extract repeated audit-only text slicing between two marker strings into one shared Python helper.

## Requirements

- The helper shall return text from the start marker through the byte before the end marker.
- The helper shall return an empty string when either marker is missing.
- Existing routing audit behavior and issue strings shall remain unchanged.
- Production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change routing truth tables.
- Do not change marker names or audit ownership boundaries.
- Do not introduce production dependency injection or coordinator restructuring.
