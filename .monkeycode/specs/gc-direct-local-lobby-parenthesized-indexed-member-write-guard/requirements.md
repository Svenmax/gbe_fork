# GC Direct Local Lobby Parenthesized Indexed Member Write Guard Requirements

## Requirement

Audit 10d shall reject parenthesized indexed member writes to the Local lobby object such as `(GBE_local_lobby).members[0].team = team`.

## Acceptance Criteria

- Given production `.cpp` source contains `(GBE_local_lobby).members[0].team = team`, when `audit_direct_local_lobby_writes(...)` runs, then it reports one direct `GBE_local_lobby` field write.
- Given production code uses existing named state helpers, when the full GC verification runs, then audit 10d continues to pass.
- The change shall not modify production lobby behavior, routing, response, push, publish, or deferred task order.

## Out of Scope

- New Local lobby apply helpers.
- Production handler refactoring.
- Message routing truth table updates.
