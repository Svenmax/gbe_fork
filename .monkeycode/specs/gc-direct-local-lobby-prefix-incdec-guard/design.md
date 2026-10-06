# Design: Direct Local Lobby Prefix Inc/Dec Guard

## Approach

Add a focused audit 10d matcher for prefix `++` / `--` applied to direct `GBE_local_lobby` field chains.

## Behavior

- Prefix increment/decrement is reported as a direct field write.
- Existing postfix and compound assignment matching remains unchanged.
- Production code remains unchanged.

## Risk

Low. The matcher is scoped to `GBE_local_lobby` field chains.
