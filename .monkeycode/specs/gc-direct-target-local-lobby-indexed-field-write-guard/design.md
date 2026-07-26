# Design: Direct Target Local Lobby Indexed Field Write Guard

## Approach

Add a regression test for target-client indexed field writes on `GBE_local_lobby`.

## Behavior

- `options.client_target->GBE_local_lobby.members[0] = member` is reported as a direct field write.
- Audit implementation remains unchanged because the indexed matcher includes the optional target prefix.
- Production code remains unchanged.

## Risk

Low. The change only adds test and documentation coverage for an already-supported matcher shape.
