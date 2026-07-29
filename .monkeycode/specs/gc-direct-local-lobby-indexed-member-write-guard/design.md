# Design: Direct Local Lobby Indexed Member Write Guard

## Approach

Add a focused audit 10d matcher for `GBE_local_lobby` field chains followed by a subscript, member chain, and write operator.

## Behavior

- `GBE_local_lobby.members[0].team = team` is reported as a direct field write.
- Existing indexed field write matching remains unchanged.
- Production code remains unchanged.

## Risk

Low. The matcher is scoped to direct `GBE_local_lobby` subscript-member writes.
