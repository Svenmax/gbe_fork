# Design: Direct Target Local Lobby Field Variant Guard

## Approach

Add a focused unit regression for target pointer Local lobby field-write variants already covered by audit 10d matching.

## Behavior

- Reuse the existing direct field write diagnostic.
- Keep audit implementation unchanged.
- Keep production code unchanged.

## Risk

Low. The change is test-only plus documentation.
