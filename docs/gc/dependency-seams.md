# GC Dependency Seams And Error Boundaries

This document records dependency touchpoints that should stay behind narrow coordinator seams during Dota GC maintenance.

## External Dependency Touchpoints

| Dependency | Current Touchpoints | Category | Governance Rule |
| --- | --- | --- | --- |
| `Settings` | Local SteamID/account/name reads, local lobby writeback, installed mod lookup, app id checks | Pure planner input or coordinator runtime state | Snapshot values before pure planning; writes stay in coordinator/executor paths |
| `Steam_Client` | Server GC lookup, matchmaking access, peer GC lookup, game-server coordinator access | Executor side-effect dependency | Access through coordinator methods or small local seams near the side effect |
| Network broadcast | `network->sendToAll`, `network->sendToAllGameservers`, own IP lookup | Executor side-effect dependency | Keep broadcasts after local response/save steps where tests define order |
| Server GC forward | `GBE_PushDotaPlayerEquippedItemsCacheToGC`, `server_gc->push_incoming_message` | Executor side-effect dependency | Full CacheSubscribed must precede server GC emsg 21/26 item updates |
| Item persistence | `save_items_to_file` | Executor side-effect dependency | Pure planners return save intent; executor calls `GBE_SaveDotaItemsFromExecutor(reason)` or an equivalent named seam |
| Lobby publish | `GBE_PublishSharedDotaLobbyState`, `GBE_SendDotaPracticeLobbyDetailsUpdate`, snapshot replay | Executor side-effect dependency | Reason strings are stable semantic labels and are covered by handler tests for high-risk paths |
| Global runtime state | Local lobby, reconnect context, pending reset/finalize flags, shared lobby state | Global runtime state | Use helper/accessor seams where available; avoid holding references across action execution |

## Persistence Seam

`GBE_SaveDotaItemsFromExecutor(reason)` is the named persistence seam for Dota executor paths. It records the executor reason in GC debug logs and delegates to `save_items_to_file`.

The equip handler side-effect order is expected to remain:

1. SO update response.
2. Direct equip response.
3. Item persistence seam.
4. Server GC forward.
5. Network broadcast.
6. Lobby snapshot refresh.

Handler tests observe the `SaveItemsToFile` action in the same position through the test wrapper.

## Ownership And Lifetime

- `GBE_local_lobby` is coordinator-owned runtime state; pure helpers should receive snapshots or scalar fields.
- `items` is coordinator-owned inventory state; planners may mutate only the provided `items` vector in the current coordinator execution scope.
- Reconnect context and pending flags are coordinator-owned lifecycle state; use function-level helpers instead of direct extern access.
- Template/replay bytes are owned by the template replay or payload helper boundary; handlers should call helper functions.
- Do not retain references or pointers to mutable coordinator state across action execution.

## Error Boundaries

Keep parsing/building failures close to the helper that understands the payload shape:

- Wire/proto helper failure should return `false` or an empty optional-style result and log through proto-boundary diagnostics when useful.
- Handler failure should decide whether to fall back, suppress, or push a minimal success/failure response.
- Coordinator/executor paths should own side-effect ordering and final debug logs.

## Adding A New Seam

When adding a new seam, include:

- A name that describes the side effect or lifecycle intent.
- A stable reason string when the seam produces observable actions.
- A handler/offline test when the side effect order is high risk.
- Audit baseline updates when the seam intentionally changes high-risk side-effect counts.
