# Design: Direct Local Lobby Forwarding Alias Guard

## Approach

Extend the audit 10d mutable alias matcher so the reference token accepts `&&` before `&` and `*`.

## Behavior

- `auto&& lobby = GBE_local_lobby` is reported as a mutable alias.
- Existing alias diagnostics and production code remain unchanged.

## Risk

Low. The matcher is still scoped to aliases assigned from `GBE_local_lobby`.
