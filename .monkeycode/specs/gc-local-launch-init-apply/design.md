# Design: GC Local Launch Init Apply

## Approach

Add `apply_launch_init_plan(GBE_LocalLobby &, const LaunchInitPlan &)` in `gbe::dota_lobby_state` and replace the direct 7041 lifecycle handler assignment with this helper.

## Behavior Preservation

- `compose_launch_init_plan(...)` remains the planner for match id, server id, connect, start time, and launch phase.
- The lifecycle handler still owns action execution around the Local apply boundary.
- Custom game setup and standard launch response flow remain unchanged.

## Tests

- Extend `gbe_dota_lobby_state_test` to assert that the helper applies launch runtime metadata and launch phase from the plan lobby.
