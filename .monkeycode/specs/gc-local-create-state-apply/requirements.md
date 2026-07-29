# Requirements: GC Local Create State Apply

## Scope

Centralize the 7038 create-plan Local lobby snapshot write behind a named state helper while preserving production protocol order.

## Requirements

- The helper shall apply the full `CreateLobbyStateApplyPlan::lobby` snapshot to `GBE_local_lobby`.
- The create handler shall continue to apply generation after the create-plan snapshot.
- The create handler shall preserve custom-game normalization, arcade member slot normalization, reconnect context side effects, create actions, publish, response, and logs in their existing order.
- The change shall not modify routing inventory or shared Store generation gate behavior.
