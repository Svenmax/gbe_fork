# Requirements: GC Local Join Merge Apply

## Scope

Centralize the 7044 join-plan Local lobby snapshot write behind a named state helper while preserving production protocol order.

## Requirements

- The helper shall apply the full `JoinLobbyMergePlan::lobby` snapshot to `GBE_local_lobby`.
- The join handler shall continue to advance and write generation around the join merge boundary exactly as before.
- Generic lobby join, settings sync, local member data, publish, cache subscription record, response, and logs shall keep their existing order.
- The change shall not modify routing inventory or shared Store generation gate behavior.
