# Requirements: GC Local Launch Init Apply

## Scope

Centralize the 7041 launch-init Local lobby snapshot write behind a named state helper while preserving production protocol order.

## Requirements

- The helper shall apply the full `LaunchInitPlan::lobby` snapshot to `GBE_local_lobby`.
- The lifecycle handler shall continue to execute `LaunchPeripheralReset` before the Local snapshot apply.
- Shared publish, custom game setup flow, rich presence, persona state, response, and logs shall keep their existing order.
- The change shall not modify routing inventory or shared Store generation gate behavior.
