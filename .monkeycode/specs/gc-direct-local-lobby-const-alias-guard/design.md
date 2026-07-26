# Design: Direct Local Lobby Const Alias Guard

## Approach

Refine the mutable alias matcher to avoid matching aliases preceded by `const` and add a focused acceptance regression.

## Behavior

- `const auto& lobby = GBE_local_lobby` is accepted.
- `auto& lobby = GBE_local_lobby` remains rejected.
- Production code remains unchanged.

## Risk

Low. The change narrows a new audit matcher to its intended mutable-alias scope.
