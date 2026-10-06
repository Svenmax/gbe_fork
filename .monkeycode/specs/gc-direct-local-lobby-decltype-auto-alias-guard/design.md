# Design: Direct Local Lobby Decltype Auto Alias Guard

## Approach

Add a focused audit 10d matcher for `decltype(auto)` declarations initialized from parenthesized `GBE_local_lobby`.

## Behavior

- `decltype(auto) lobby = (GBE_local_lobby)` is reported as a mutable alias.
- Unparenthesized `decltype(auto) lobby = GBE_local_lobby` is not targeted by this guard.
- Production code remains unchanged.

## Risk

Low. The matcher is scoped to a specific aliasing form assigned from `GBE_local_lobby`.
