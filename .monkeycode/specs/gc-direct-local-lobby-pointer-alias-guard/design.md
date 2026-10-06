# Design: Direct Local Lobby Pointer Alias Guard

## Approach

Add a focused unit regression for mutable pointer alias detection already covered by audit 10d.

## Behavior

- `GBE_DotaLobbyState* lobby = &GBE_local_lobby` is rejected as a mutable alias.
- Audit implementation remains unchanged.
- Production code remains unchanged.

## Risk

Low. The change is test-only plus documentation.
