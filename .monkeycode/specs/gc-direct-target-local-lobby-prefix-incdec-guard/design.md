# Design: Direct Target Local Lobby Prefix Inc/Dec Guard

## Approach

Add a regression test for target-client prefix increment on `GBE_local_lobby` field chains.

## Behavior

- Target pointer prefix increment/decrement is reported as a direct field write.
- Audit implementation remains unchanged because the existing prefix matcher includes the optional target prefix.
- Production code remains unchanged.

## Risk

Low. The change only adds test and documentation coverage for an already-supported matcher shape.
