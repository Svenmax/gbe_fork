# Dota GC Future Refactor Plan

This document records the agreed follow-up direction after the GC coordinator refactor review. The goal is not to make the code look cleaner for its own sake. The goal is to reduce real maintenance risk: make bugs easier to locate, make regressions harder to introduce, and make behavior easier to test and review.

## Decision Standard

A follow-up refactor is worth doing only when it satisfies at least one of these criteria:

- It makes a known class of bugs easier to locate.
- It makes an existing error pattern harder to repeat.
- It makes important behavior easier to cover with focused offline tests.
- It makes review more reliable by replacing ambiguous field mutation with named lifecycle or side-effect intent.

A refactor should not be done merely to reduce line count, remove all globals, make the design look more object-oriented, or hide data behind mechanical getters and setters.

## Current Position

The current branch has already landed the most important pre-merge cleanup:

- PR CI runs fast GC verification.
- Duplicated local/shared/last-launch reset logic is centralized through `Steam_Game_Coordinator::GBE_ClearDotaLobbyRuntimeState()`.
- `GBE_vpk_loot_data` no longer exposes a writable `extern`.
- `GBE_last_dota_server_hello_context` is TU-local and accessed through existing accessors.
- Agent-private process docs were removed from product directories; durable guidance was moved under `docs/gc/`.
- Full GC verification passed after the cleanup.

This is a good stopping point for the current PR. Further work should be split into small, separately reviewable follow-up PRs.

## Do Not Do Next

Do not immediately split `Steam_Game_Coordinator` into Dota sub-objects. The class is still a large integration host, but the required state and side-effect boundaries are not stable enough yet. Splitting it now would likely create several smaller objects that still call back into each other and still share the same implicit global state.

Do not mechanically replace `GBE_shared_dota_lobby_state.foo` with `GetSharedState().foo` or a large set of `SetFoo(...)` helpers. That would be fake encapsulation: risk without much maintenance benefit.

Do not combine shared-state facade work, dependency seam work, and object decomposition in the same PR.

## Recommended Sequence

### P1. Lifecycle Tests And Semantic Reset Helpers

Start by strengthening tests around existing behavior before changing more boundaries.

Focus paths:

- Normal signout finalize after cache unsubscribe.
- Abandon finalize.
- Cache unsubscribed cleanup.
- Player postgame cleanup.
- Host client GC postgame observation, where the client must not clear shared state needed by the server GC.
- Stale postgame chat leave, which intentionally clears local chat state without broad runtime reset.
- Reconnect paths that preserve reconnect context.

Desired tests:

- Message ordering remains stable.
- `GBE_local_lobby`, `GBE_shared_dota_lobby_state`, and last pushed launch state are cleared only on paths that should clear them.
- Special paths that intentionally preserve shared or reconnect state keep doing so.

Only after tests are in place should more lifecycle helpers be added. Good helper names should encode why the transition exists, for example:

- `GBE_ClearDotaRuntimeForNormalSignout(...)`
- `GBE_FinalizeDotaPostgameCleanup(...)`
- `GBE_ClearDotaRuntimeForReset(...)`

These helpers must preserve existing message order and should be introduced one path at a time.

### P1. Side-Effect Seams For Push, Broadcast, And Response Ordering

Many Dota handlers and coordinators both mutate state and emit side effects. The dangerous part is not that helpers are long; it is that side effects are hard to see and hard to assert.

Candidate side effects:

- `push_incoming_now(...)`
- `push_incoming_response(...)`
- lobby details updates
- network broadcast
- server GC forwarding
- rich presence updates

Start with tests and recorder assertions. Then wrap only obvious repeated or high-risk operations in named helpers that preserve message content and ordering. A future seam can evolve toward a `DotaGcSideEffects` style interface, but the first steps should remain small.

Review target: a reviewer should be able to see whether a path queued cache unsubscribe 25, published a lobby snapshot, sent 8247, or preserved wrapped/session/source-job metadata without reading every field assignment.

### P1. Read-Only Shared Lobby State Facade

`GBE_shared_dota_lobby_state` is valuable to refactor, but it is also dangerous because it is an implicit protocol between server GC, client GC, lifecycle cleanup, postgame, reconnect, and lobby state publishing.

The first facade step should be read-only only. Examples:

- `GBE_HasSharedDotaLobbyState()`
- `GBE_GetSharedDotaLobbyStateSnapshot()`
- `GBE_GetSharedDotaLobbyIdOrZero()`
- `GBE_IsSharedDotaLobbyActive()`

Replace only clearly read-only call sites. Do not return a mutable reference. Do not change publish or clear behavior in the same step.

### P2. Shared Lobby State Clear Facade

After read-only access is stable, centralize clear paths mechanically.

The first clear helper should be behavior-equivalent:

```cpp
void GBE_ClearSharedDotaLobbyState()
{
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
}
```

Do not add preserve logic, extra state clearing, or new ordering behavior in the first PR. If reason strings are added, they must not affect behavior and should be covered by the audit where relevant.

Later, if tests justify it, split clear helpers by lifecycle intent:

- clear for normal signout
- clear for player postgame cleanup
- clear for full runtime reset

This is where the facade starts carrying real maintenance value: direct field clearing becomes named lifecycle intent.

### P2. Settings, Rich Presence, And Server/Client Lookup Boundaries

After lifecycle tests and side-effect recorders are stronger, centralize the next risky dependencies.

Settings and rich presence candidates:

- `settings->get_lobby()`
- `settings->set_lobby(k_steamIDNil)`
- practice lobby launch rich presence clear/update
- local Steam ID and lobby owner lookups

Server/client GC lookup candidates:

- `get_steam_client()` access patterns
- client GC target selection
- gameserver GC ownership checks
- host client postgame skip checks

The first helpers should be read-only or behavior-equivalent wrappers, for example:

- `GBE_HostHasActiveDotaServerLobby(lobby_id)`
- `GBE_GetDotaClientCoordinatorIfPresent()`
- `GBE_ClearSettingsLobbyForDotaSignout(...)`

The point is to make host/client and settings side effects visible without changing their order.

### P2/P3. Inventory And Template Replay Data Flow

These areas are lower priority unless they become active bug sources.

Inventory/VPK direction:

- Keep mutation centralized.
- Separate load/cache/update behavior from read-only query behavior.
- Add focused tests before changing data ownership.

Template replay direction:

- Do not move binary templates for appearance alone.
- If template replay keeps changing, separate dispatch metadata from replay behavior while keeping fixture output stable.
- Any blob movement must be fixture-protected.

### P3. Natural Dota Sub-Object Extraction

Only after state boundaries, side-effect seams, and lifecycle tests are stable should Dota sub-objects be considered.

A new object is justified only if:

- It does not need the entire `Steam_Game_Coordinator *` to do its work.
- It depends on a small explicit context or service set.
- It can be tested directly.
- It reduces coupling instead of merely moving member functions to a new class.

Possible future objects, if the dependencies naturally converge:

- `DotaLobbyRuntime`
- `DotaLobbyLifecycleCoordinator`
- `DotaGcSideEffects`
- `DotaInventoryService`
- `DotaTemplateReplayService`
- `DotaSharedLobbyStateStore`

Do not force this structure upfront.

## Stop Conditions

Stop refactoring when any of these becomes true:

- The change no longer makes bugs easier to locate or prevent.
- The abstraction requires a long explanation but does not reduce concrete risk.
- Tests are not added before risky behavior changes.
- Review becomes harder because call chains are longer or intent is less direct.
- A PR starts mixing state boundary, side-effect boundary, and object decomposition work.

The target is not a fully refactored architecture. The target is a maintainable system where future Dota GC bugs can be localized to state, lifecycle, side effect, replay, or dependency boundaries with confidence.

## Verification Rule

Every follow-up PR in this area should run:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

Where possible, add or update focused offline tests before changing lifecycle, shared state, or side-effect behavior.
