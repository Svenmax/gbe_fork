# Dota GC Concurrency Ownership

## Scope

This contract records the current thread ownership and synchronization boundaries for the state involved in lobby publication, reconnect interception, callback delivery, and delayed GC work. P11.1 documents the existing model. P11.2 and P11.3 use this baseline to refine lock order and instance synchronization without changing protocol behavior.

## State Ownership

| State | Owner | Synchronization domain | Allowed access | Boundary rule |
| --- | --- | --- | --- | --- |
| Shared lobby store | Process-wide `gbe::dota_lobby_state::Store` returned by `GBE_GetSharedDotaLobbyStateStore()` | Existing process `global_mutex`, held internally by every store operation | Value snapshots, complete publish, monotonic publish, clear, copy-update, generation compare-update | Production callers receive value snapshots. Store mutators transform only the copied `Snapshot`; callback, network, coordinator, logging, and other external API calls stay outside the mutator. |
| Recent reconnect context | Process-wide fallback context in `steam_game_coordinator.cpp` | Caller-held `global_mutex` domain | `GBE_GetRecentDotaReconnectContext()`, `GBE_SetRecentDotaReconnectContext()`, `GBE_ClearRecentDotaReconnectContext()` | Access occurs from GC/coordinator paths already serialized by `global_mutex`. The context is copied in and out as one value. P11.2 must preserve this domain or introduce a dedicated lock before allowing independent-thread access. |
| Serialized connection state | One `GBE_DotaSerializedConnectionState` and `GBE_DotaSerializedConnectionSynchronizer` per `Steam_Networking_Sockets_Serialized` instance | Dedicated instance mutex acquired inside the caller-held process lock for reconnect prepare | `GBE_PrepareDotaReconnectPostConnectionState()` and state methods called from the owning serialized sockets instance | The instance lock protects context probe cache and state mutation through dedup reservation. It is released before the process lock and external effects. State never crosses instances and separate client/server serialized instances have independent synchronization domains. |
| Reconnect adapter probe cache | One `GBE_DotaReconnectNetworkAdapter` per serialized sockets instance | Same dedicated instance mutex as serialized connection state | `get_context()` updates the one-second recovery probe cache during locked prepare | Cache fields stay private to the adapter instance and are read or updated only while the owning `PostConnectionStateMsg()` holds its instance lock. |
| Callback queue | Client/server `SteamCallBacks` and `SteamCallResults` objects owned by `Steam_Client` | Queue mutation and draining occur while `Steam_Client::RunCallbacks()` owns a `unique_lock` on `global_mutex` | `addCBResult()` copies payload and guard context; `runCallResults(process_lock)` evaluates guards and delivers ready callbacks | Queue containers remain protected while selecting and copying work. User callbacks and `cb_all` execute through one RAII unlock/relock boundary before queue traversal continues. |
| Delayed reconnect callback | `SteamCallResults` entry created by `GBE_DotaReconnectNetworkAdapter::queue_game_server_change()` | Callback queue domain | Copied `GameServerChangeRequested_t`, delay, and copied generation guard | Execution validates the current lobby generation immediately before delivery. Stale work is discarded without invoking the callback. |
| Delayed GC message | Per-`Steam_Game_Coordinator` `pending_messages` queue | Coordinator paths currently serialized by `global_mutex` | `push_incoming()` queues copied payload and generation metadata; coordinator callback processing drains it into `incoming_messages` | Dota state-applying messages capture lobby ID and generation at enqueue time. Drain rejects stale generation before applying queued lobby state or publishing the message. |
| Deferred lifecycle slot | Per-`Steam_Game_Coordinator` `GBE_DotaDeferredTaskSlot` fields | Same coordinator `global_mutex` domain | Set, clear, and consume through the coordinator helper methods | Consume compares captured lobby ID and generation with current state, clears the slot, and reports current, stale, or empty. |

## Current Entry Domains

- `Steam_Game_Coordinator::SendMessage_()`, `IsMessageAvailable()`, and `RetrieveMessage()` enter through `global_mutex`.
- `Steam_Networking_Sockets_Serialized::PostConnectionStateMsg()` acquires `global_mutex`, then its instance mutex, prepares and reserves reconnect state, releases the instance mutex followed by `global_mutex`, then executes direct network and callback queue effects.
- `Steam_Client::RunCallbacks()` holds `global_mutex` while networking, run-every-callback work, queue selection, and queue cleanup execute.
- `SteamCallResults::runCallResults()` receives the owning `unique_lock`, releases it through an exception-safe boundary around user callbacks and `cb_all`, and reacquires it before continuing internal queue traversal.
- Store methods acquire `global_mutex` internally. Recursive acquisition is currently supported because the process lock is a `std::recursive_mutex`.

## Lock Boundary Rules

1. Capture shared lobby state as a value before business decisions and external effects.
2. Keep store mutators deterministic and local to the supplied snapshot.
3. Copy callback payload, execution guard, and delayed message metadata when work is queued.
4. Validate lobby generation at the final asynchronous execution boundary.
5. Keep serialized connection state and reconnect probe cache within one serialized sockets instance.
6. Apply serialized reconnect lock order as process `global_mutex` then instance mutex; release both before network effects. Prepare reserves dedup keys before either lock is released.

## Known Follow-Up Boundaries

- Recent reconnect context relies on caller-held `global_mutex`; a future dedicated synchronization owner may narrow this process-wide dependency.
- Any future entrypoint that touches serialized connection state or the adapter probe cache must use the same instance synchronizer and lock order.

## Bounded Stress Gate

`gbe_dota_concurrency_stress_test` runs one writer and four readers across 1,000 generations. The writer combines monotonic publish, matching and stale compare/update, and clear operations. Each reader validates one immutable snapshot version and builds a reconnect context from that same value. The gate requires zero torn snapshots, zero context identity drift, zero stale mutator execution, and a complete final generation.
