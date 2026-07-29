# GC Direct Target Local Lobby Parenthesized Field Write Guard Requirements

## Requirement

Audit 10d shall reject parenthesized direct field writes to a target Local lobby object such as `(options.client_target->GBE_local_lobby).state = 1u`.

## Acceptance Criteria

- Given production `.cpp` source contains `(options.client_target->GBE_local_lobby).state = 1u`, when `audit_direct_local_lobby_writes(...)` runs, then it reports one direct `GBE_local_lobby` field write.
- Given production code uses existing named state helpers, when the full GC verification runs, then audit 10d continues to pass.
- The change shall not modify production lobby behavior, routing, response, push, publish, or deferred task order.

## Out of Scope

- New Local lobby apply helpers.
- Production handler refactoring.
- Message routing truth table updates.
