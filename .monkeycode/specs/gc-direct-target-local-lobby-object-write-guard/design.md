# Design: Direct Target Local Lobby Object Write Guard

## Approach

Add a focused unit regression to `DirectLocalLobbyWriteAuditTest` for full object writes through `options.client_target->GBE_local_lobby`.

## Behavior

- Reuse the existing `audit_direct_local_lobby_writes(...)` regex and diagnostic.
- Keep production files unchanged.
- Keep the audit category as direct `GBE_local_lobby` object writes.

## Risk

Low. The change only increases test coverage for existing audit behavior.
