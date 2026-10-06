# Design: Direct Local Lobby Explicit Type Alias Guard

## Approach

Extend the audit 10d mutable alias matcher to include the concrete Local lobby type `GBE_LocalLobby`.

## Behavior

- `GBE_LocalLobby&` and `GBE_LocalLobby*` aliases to `GBE_local_lobby` are reported as mutable aliases.
- Existing `const` alias behavior remains unchanged.
- Production code remains unchanged.

## Risk

Low. The matcher remains scoped to explicit aliases assigned from `GBE_local_lobby`.
