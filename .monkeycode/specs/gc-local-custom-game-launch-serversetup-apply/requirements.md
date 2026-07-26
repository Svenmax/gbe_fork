# Requirements: GC Local Custom Game Launch Serversetup Apply

## Scope

Centralize the 7041 custom game SERVERSETUP Local lobby snapshot write behind a named state helper while preserving production protocol order.

## Requirements

- The helper shall apply the full `CustomGameLaunchSetupPlan::serversetup_lobby` snapshot to `GBE_local_lobby`.
- The launch coordinator shall continue to build and push READYUP details before the Local SERVERSETUP snapshot apply.
- Shared publish, SERVERSETUP details push, launch phase mark, steam-auth ack queue, and logs shall keep their existing order.
- The change shall not modify routing inventory or shared Store generation gate behavior.
