# Design: Direct Local Lobby Indexed Prefix Write Guard

## Approach

Add a focused audit 10d matcher for prefix `++` / `--` applied to `GBE_local_lobby` indexed field chains.

## Behavior

- `++GBE_local_lobby.members[0].team` is reported as a direct field write.
- Existing indexed postfix and assignment matching remains unchanged.
- Production code remains unchanged.

## Risk

Low. The matcher is scoped to direct `GBE_local_lobby` indexed prefix writes.
