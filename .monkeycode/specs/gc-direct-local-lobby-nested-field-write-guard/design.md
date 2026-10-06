# Design: Direct Local Lobby Nested Field Write Guard

## Approach

Extend the audit 10d direct field write regex to match a dotted member chain after `GBE_local_lobby.`.

## Behavior

- `GBE_local_lobby.state = ...` remains rejected.
- `GBE_local_lobby.custom_game.game_id = ...` is rejected.
- Target pointer forms continue to use the same diagnostic category.
- Production code remains unchanged.

## Risk

Low. The change only broadens audit detection for direct Local lobby write syntax.
