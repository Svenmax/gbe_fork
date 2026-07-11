# Dota GC Internal Interfaces

## Purpose

This document lists the internal contracts most relevant to GC maintenance. It describes ownership and extension rules; concrete signatures remain authoritative in the referenced headers.

## Typed Request Context

Location:

- `dll/gbe_dota_request_router.h`
- `dll/gbe_dota_gc_router.h`
- `dll/gbe_dota_handler_registry.h`

`DotaGcRequestContext` normalizes direct and wrapped requests. Maintain these properties:

- Inner message ID and body are independent from outer wrapping.
- Source and target job identifiers preserve their wire semantics.
- Wrapped-session forwarding follows the registry entry policy.
- Request path is explicit and participates in registry matching.
- Unknown paths do not match production entries.

## Handler Registry

Location: `dll/gbe_dota_handler_registry.h` and the production table/dispatcher owner in `dll/gbe_dota_post_login_dispatcher.cpp`.

Each registry entry binds:

- Message ID.
- Request mode.
- Session policy.
- Lifecycle class.
- Adapter function.
- Handler identity.
- Fixture metadata.

Extension rules:

1. Add the mapping to the canonical production table.
2. Preserve direct and wrapped mode semantics.
3. Provide a live smoke or replay fixture for high-risk lifecycle entries.
4. Keep adapter logic limited to signature and context adaptation.
5. Run registry tests and architecture audits.

## Lifecycle Executor

Location:

- `dll/gbe_dota_lifecycle_actions.h`
- `dll/gbe_dota_lifecycle_actions.cpp`
- `dll/gbe_dota_custom_game_lifecycle_coordinator.cpp`

The production executor consumes an ordered `GBE_DotaActionList`. It owns side-effect execution and conditional action policy.

Planner rules:

- Produce values and typed actions.
- Preserve protocol-visible ordering.
- Avoid network, callback, Store, logging, filesystem, or mutable global access.

Executor rules:

- Execute actions serially.
- Preserve client/gameserver routing.
- Honor previous-action success conditions.
- Keep runtime-update fallback conditions stable.
- Apply push failure policy consistently.

## Shared Lobby Store

Location:

- `dll/gbe_dota_lobby_state_store.h`
- `dll/gbe_dota_lobby_state_store.cpp`

The Store is the production shared-lobby state contract. Consumers use value snapshots and generation-aware mutation methods.

Extension rules:

- Capture one snapshot per business operation.
- Keep mutators deterministic and local to the candidate value.
- Add semantic Store methods only when a repeated state boundary exists.
- Preserve stale-generation rejection.
- Use `compare_clear(expected_generation)` for production lifecycle cleanup.
- Preserve tombstone generation so stale clear and same-generation republish cannot replace newer lifecycle history.
- Reserve `clear()` for test or process-level forced reset.
- Execute external effects after the Store operation returns.

## Locator Binding

Location: `dll/gbe_dota_locator.h` and `dll/gbe_dota_locator.cpp`.

`gbe::dota::LocatorBindingGuard` binds the application-owned lobby Store and runtime state as one lifetime unit. `Steam_Client` creates the guard before coordinator construction and resets it after coordinator destruction. The guard rolls back the Store binding when runtime-state binding fails and unbinds both locators in reverse order.

## Lifecycle State Machine

Location: `dll/gbe_dota_lifecycle_state_machine.h`.

The state machine exposes pure transition functions for lifecycle, generation, reconnect, runtime updates, and teardown gates.

Extension rules:

- Add typed state or event vocabulary centrally.
- Classify every new state/event pair.
- Preserve compile-time completeness.
- Update example, property, differential, and production-path tests.
- Refresh architecture investment evidence after a major state-space expansion.

## Generation Counter

Location: `dll/gbe_dota_lobby_generation.h`.

Generation is an epoch counter with explicit lifecycle boundaries. Zero represents an unallocated lifecycle. Advance is monotonic and saturating.

Extension rules:

- Use the coordinator canonical advancement path.
- Capture generation when asynchronous work is created.
- Validate generation at final execution.
- Preserve lobby ID checks for state-applying delayed messages.
- Avoid reconstructing counters outside the documented recovery synchronization points.

## Reconnect Ports

Location:

- `dll/gbe_dota_reconnect_network.h`
- `dll/gbe_dota_reconnect_network.cpp`
- `dll/gbe_dota_reconnect_network_adapter.cpp`
- `dll/gbe_dota_reconnect_context.h`

The reconnect layer depends on three capabilities:

- `GBE_DotaReconnectContextProvider`.
- `GBE_DotaReconnectDirectConnector`.
- `GBE_DotaReconnectCallbackQueue`.

The pure preparation function updates reconnect state and returns a plan. The effect executor calls the direct connector and callback queue.

Extension rules:

- Keep context mapping in the canonical reconnect context owner.
- Keep `ConnectByIPAddress` inside the production adapter.
- Reserve dedup keys while locks are held.
- Execute network and callback effects after lock release.
- Preserve generation guards on queued callbacks.

## Diagnostic Event

Location: `dll/gbe_dota_diagnostic_event.h`.

Typed diagnostic events have stable fields:

```text
event
reason
source
lobby_id
generation
server_id
endpoint
decision
message_id
job_id
```

Extension rules:

- Add typed `Reason` or `Source` values to the centralized mapping.
- Keep stable string serialization unique.
- Update focused serialization tests.
- Exclude raw payloads, passwords, sessions, and unrestricted state dumps.
- Keep Audit 8 inventory checks passing.

## Callback Execution Guard

Location: callback and call-result infrastructure used by reconnect delivery.

The execution guard is copied with queued callback work and evaluated immediately before callback dispatch. It allows a callback queued in a valid generation to become stale before execution.

Extension rules:

- Copy guard data when queueing.
- Evaluate against current state at dispatch.
- Run user callbacks outside the process lock.
- Reacquire the owning lock before continuing internal queue traversal.
