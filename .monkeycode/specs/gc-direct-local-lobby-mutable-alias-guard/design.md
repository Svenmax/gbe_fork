# Design: Direct Local Lobby Mutable Alias Guard

## Approach

Add a focused audit 10d matcher for mutable reference or pointer aliases initialized from `GBE_local_lobby`.

## Behavior

- Matched aliases get a dedicated mutable alias diagnostic.
- Existing direct write diagnostics remain unchanged.
- Production code remains unchanged.

## Risk

Low. The matcher is scoped to mutable `auto` or `GBE_DotaLobbyState` aliases initialized directly from `GBE_local_lobby`.
