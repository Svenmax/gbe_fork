# GC Dependency Seam and Error Boundary Governance

## External Dependency Touchpoints

| Dependency | Current Touchpoints | Category | Governance Rule |
| --- | --- | --- | --- |
| `Settings` | Local SteamID/account/name reads, local lobby writeback, installed mod lookup, app id checks | Pure planner input or coordinator runtime state | Snapshot values before pure planning; writes stay in coordinator/executor paths |
| `Steam_Client` | Server GC lookup, matchmaking access, peer GC lookup, gameserver coordinator access | Executor side effect dependency | Access through coordinator methods or small local seams near the side effect |
| Network broadcast | `network->sendToAll`, `network->sendToAllGameservers`, own IP lookup | Executor side effect dependency | Keep broadcasts after local response/save steps where tests define order |
| Server GC forward | `GBE_PushDotaPlayerEquippedItemsCacheToGC`, `server_gc->push_incoming_message` | Executor side effect dependency | Full CacheSubscribed must precede server GC emsg 21/26 item updates |
| Item persistence | `save_items_to_file` | Executor side effect dependency | Pure planners return save intent; executor calls `GBE_SaveDotaItemsFromExecutor(reason)` or an equivalent named seam |
| Lobby publish | `GBE_PublishSharedDotaLobbyState`, `GBE_SendDotaPracticeLobbyDetailsUpdate`, snapshot replay | Executor side effect dependency | Reason strings are stable semantic labels and are covered by handler tests for high-risk paths |
| Global runtime state | Local lobby, reconnect context, pending reset/finalize flags, shared lobby state | Global runtime state | Use function-level accessors where available; avoid holding references across action execution |

## Current Minimal Seam

`GBE_SaveDotaItemsFromExecutor(reason)` is the first named persistence seam for Dota executor paths. It records the executor reason in GC debug logs and delegates to `save_items_to_file`. Handler tests declare the same seam in the coordinator stub, preserving the existing `SaveItemsToFile` recorder event. The equip handler now calls this seam at the same point in the established side-effect order:

1. SO update response.
2. Direct equip response.
3. Item persistence seam.
4. Server GC forward.
5. Network broadcast.
6. Lobby snapshot refresh.

Existing handler tests continue to observe the `SaveItemsToFile` action in the same position through the test wrapper.

## Ownership and Lifetime

- `GBE_local_lobby` is coordinator-owned runtime state; pure helpers receive snapshots or scalar fields.
- `items` is coordinator-owned inventory state; planners may mutate only the provided `items` vector in the current coordinator execution scope.
- Reconnect context and pending flags are coordinator-owned lifecycle state; use the existing function-level helpers instead of direct extern access.
- Template/replay bytes are owned by the template replay or payload helper boundary; handlers should call helper functions.
- Do not retain references or pointers to `items`, lobby members, template buffers, or settings-derived strings across queued actions.

## Persistence Boundary

- Persistence belongs to coordinator/executor code after successful local mutation.
- Pure request parsing, DTO parsing, and decision helpers return intent and never write files.
- New save sites should use a named reason so future tests and logs can identify the business trigger.

## Error and Fallback Classes

| Class | Response Behavior | Logging | State Mutation |
| --- | --- | --- | --- |
| Parse failure or malformed request | Prefer existing fallback response behavior for that handler; no extra response when current behavior is silent | Debug log with emsg/context when already available | No mutation unless prior behavior required it |
| Missing lobby | Return true and ignore for lobby/match flows that already treat missing lobby as benign | Debug log with emsg and lobby id when known | No mutation |
| Missing item | Preserve existing response behavior; equip parse failure stays side-effect free | Debug log or test assertion as appropriate | No save, callback, server GC forward, or broadcast |
| Server GC unavailable | Continue local client behavior; skip server-GC-specific forward | Debug log when path is observable | Local state remains authoritative |
| Template patch failure | Skip malformed replay/payload generation and keep fallback path | Debug log with template owner and request emsg | No partial state mutation after failed patch |

## Replay and Template Performance

- Avoid repeated large blob copies in handlers; keep canned bytes behind helper boundaries.
- Patch helpers should mutate local message buffers and return match/size metadata for focused tests.
- Fixture changes must update replay expected files and relevant focused tests in the same change.
- Large canned payloads should include owner/use comments in the owning helper or the ownership document.
