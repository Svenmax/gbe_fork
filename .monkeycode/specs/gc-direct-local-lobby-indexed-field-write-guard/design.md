# Design: Direct Local Lobby Indexed Field Write Guard

## Approach

Add a focused audit 10d matcher for a `GBE_local_lobby` field chain followed by a subscript and a write operator.

## Behavior

- `GBE_local_lobby.members[0] = member` is reported as a direct field write.
- Existing field write and mutating method matching remains unchanged.
- Production code remains unchanged.

## Risk

Low. The matcher is scoped to direct `GBE_local_lobby` subscript writes.
