# Design: GC Local Custom Game Launch Serversetup Apply

## Approach

Add `apply_custom_game_launch_serversetup_plan(GBE_LocalLobby &, const CustomGameLaunchSetupPlan &)` in `gbe::dota_lobby_state` and replace the direct SERVERSETUP assignment in `GBE_SendDotaCustomGameLaunchSetupFlow(...)`.

## Behavior Preservation

- `compose_custom_game_launch_setup_plan(...)` remains the planner for READYUP and SERVERSETUP snapshots.
- The launch coordinator still owns READYUP details push, Local SERVERSETUP apply, shared publish, SERVERSETUP details push, launch phase mark, and queued steam-auth ack ordering.

## Tests

- Extend `gbe_dota_lobby_state_test` to assert that the helper applies the SERVERSETUP snapshot while preserving launch runtime metadata.
