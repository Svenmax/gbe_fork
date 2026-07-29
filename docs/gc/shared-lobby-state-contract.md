# Shared Lobby State Contract

This document records the current maintenance contract for `GBE_shared_dota_lobby_state`. It is a guardrail for future refactors, not a request to hide every field behind mechanical getters.

## Why This State Is High Risk

`GBE_shared_dota_lobby_state` is not just a cache. It is an implicit protocol between server GC, client GC, payload helpers, lobby publication, reconnect decisions, postgame cleanup, and normal signout cleanup.

A small-looking write can change whether a client sees a lobby as joinable, whether reconnect payloads are built, whether postgame cleanup preserves enough state for the server GC, or whether stale chat-leave handling re-publishes a lobby that was already cleared.

The target is therefore not cosmetic encapsulation. The target is named lifecycle intent and safer review.

## Current Owners And Users

The variable is still defined as shared mutable state and declared through `dll/gbe_dota_gc_internal.h`.

Current production users include:

- `dll/steam_game_coordinator.cpp`
- `dll/gbe_dota_lobby_state_coordinator.cpp`
- `dll/gbe_dota_lobby_flow_coordinator.cpp`
- `dll/gbe_dota_lobby_launch_coordinator.cpp`
- `dll/gbe_dota_lobby_handlers.cpp`
- `dll/gbe_dota_chat_handlers.cpp`
- `dll/gbe_dota_gc_payload_helpers.cpp`
- `dll/gbe_dota_payload_lobby_helpers.cpp`
- `dll/gbe_dota_payload_item_helpers.cpp`
- `dll/gbe_dota_payload_wire_helpers.cpp`
- `dll/gbe_dota_post_login_handlers.cpp`
- `dll/gbe_dota_template_replay_handlers.cpp`

Test users include the handler and payload helper offline tests, which currently stub or inspect the same global contract.

## Current Role

`GBE_local_lobby` is the local coordinator's mutable working state.

`GBE_shared_dota_lobby_state` is the cross-coordinator snapshot used to make other Dota GC paths observe lobby state without owning the local coordinator instance. It carries lobby identity, active/valid flags, state and game-state fields, server/connect metadata, custom-game metadata, member data, and reconnect-relevant fields.

Important distinction:

- Clearing `GBE_local_lobby` removes this coordinator's local working state.
- Clearing `GBE_shared_dota_lobby_state` invalidates the shared snapshot other paths may still depend on.
- Clearing last pushed launch state affects launch/rich-presence deduplication and must not be assumed equivalent to either lobby clear.

The helper `Steam_Game_Coordinator::GBE_ClearDotaLobbyRuntimeState()` intentionally clears all three for paths that need a full runtime reset.

## Allowed Mutation Patterns Today

Until a facade exists, direct mutation is tolerated only for existing behavior-preserving paths.

Preferred current patterns:

- Publish local state through `GBE_PublishSharedDotaLobbyState(reason)`.
- Use `GBE_ClearDotaLobbyRuntimeState()` when the path already means full local/shared/last-launch runtime reset.
- Keep partial-reset exceptions explicit near the call site.
- Preserve existing message order before or after any state publish/clear.

Avoid adding new ad-hoc writes to shared fields from handlers. If a new behavior needs shared-state mutation, first decide whether it is a publish, a clear, or a read-only decision.

C1（2026-07-13）复核：生产 dll 无裸 `Store::publish/update`；shared clear 走 `compare_clear`。local 全量赋值与 publish 门面清单见 [LOCAL_LOBBY_USAGE.md](./LOCAL_LOBBY_USAGE.md)。

## Behaviors That Must Be Protected

### Normal Signout Finalize

`GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(...)` consumes the pending normal-signout cleanup and then performs runtime cleanup. It now reaches shared-state clearing through `GBE_ClearDotaLobbyRuntimeState()`.

The important contract is that normal signout can clear local lobby state, shared lobby state, and last pushed launch state after the cache-unsubscribe boundary is satisfied.

### Stale Or Postgame Chat Leave

`GBE_HandleDotaLeaveChatChannelRequest` has a special path for postgame chat leave after shared state has already been cleared. If `GBE_shared_dota_lobby_state.valid` is false, it must not publish stale local lobby state back into the shared state. It leaves the generic lobby and clears local state instead.

This is a deliberate exception. Do not replace it with a broad runtime reset unless focused tests prove the behavior remains identical.

### Host Client Postgame Observation

Some client-side postgame paths observe server-owned state. A host client must not clear shared state that the server GC still needs, an ordinary player client must clear local/shared state after sending the final details update, and an active arcade lobby must preserve state while skipping cleanup messages. Host-client ownership takes precedence when it overlaps with arcade-active state, while the active arcade runtime details-update suppression still applies to the observable push sequence. This is covered by focused postgame decision tests, `test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state`, `test_lobby_player_postgame_observation_clears_shared_state_after_details_update`, `test_lobby_arcade_active_postgame_observation_preserves_shared_state`, and `test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip`. This is one of the reasons `GBE_shared_dota_lobby_state` should not be wrapped by a coarse setter/clear API without lifecycle-specific tests.

### Reconnect Decisions

Payload helpers use shared-state fields when deriving reconnect eligibility and reconnect payloads. Clearing or partially updating the shared snapshot can change reconnect behavior even if the immediate handler still passes.

### Launch State Deduplication

Last pushed launch game state is related but separate. Full runtime reset clears it with local and shared lobby state. Partial lifecycle paths must not assume shared-state invalidation automatically handles launch-state deduplication.

## Facade Direction

The first facade step should be read-only only.

Candidate helpers:

```cpp
bool GBE_HasSharedDotaLobbyState();
GBE_SharedDotaLobbyState GBE_GetSharedDotaLobbyStateSnapshot();
uint64 GBE_GetSharedDotaLobbyIdOrZero();
bool GBE_IsSharedDotaLobbyActive();
bool GBE_IsSharedDotaArcadeLobbyActive();
GBE_DotaReconnectSharedStateSnapshot GBE_GetSharedDotaReconnectStateSnapshot();
```

Rules for the first read-only facade PR:

- Do not return a mutable reference.
- Do not change publish behavior.
- Do not change clear behavior.
- Replace only obviously read-only call sites.
- Keep `tools/run_gc_verification.sh --full` green.

The second safe step is a behavior-equivalent clear helper:

```cpp
void GBE_ClearSharedDotaLobbyState()
{
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
}
```

Do not add preserve logic in the first clear-helper PR. Lifecycle-specific helpers can come later only after tests make the intended differences explicit.

## Review Checklist

For any PR touching this state, verify:

- Does the change make bug localization easier, or is it just cosmetic?
- Is the path a read, publish, clear, or partial update?
- Does it preserve message and side-effect order?
- Could it re-publish stale local state after shared state was intentionally cleared?
- Could it make a host client clear server-needed state?
- Could it change reconnect eligibility or launch-state deduplication?
- Is there a focused offline test for the path, or should one be added first?

## Stop Rule

If a proposed facade requires broad code motion before it can preserve behavior, stop. Add focused lifecycle tests first, then make the smallest behavior-equivalent replacement.
