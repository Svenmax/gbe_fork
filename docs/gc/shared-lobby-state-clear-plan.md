# Shared Lobby State Clear Plan

This note is the planning guardrail for any future expansion of `GBE_ClearSharedDotaLobbyState()`.

The current helper is intentionally raw and behavior-equivalent:

```cpp
void GBE_ClearSharedDotaLobbyState()
{
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
}
```

Do not add lifecycle preserve logic, server/client ownership logic, logging, or side effects to this raw helper. Add named lifecycle helpers around it only after the relevant behavior is covered by tests.

## Current Clear Surface

As of this pass, production code has one direct raw clear helper definition and one behavior-equivalent runtime reset wrapper:

- `steam_game_coordinator.cpp`: `GBE_ClearSharedDotaLobbyState()` defines the raw reset.
- `GBE_ClearSharedDotaLobbyForRuntimeReset()` wraps the raw reset without adding preserve logic, logging, or side effects.
- `Steam_Game_Coordinator::GBE_ClearDotaLobbyRuntimeState()` clears local lobby state, shared lobby state through the runtime-reset wrapper, and last pushed launch state together.

The important current behavior is that runtime reset performs a full local/shared/launch-state reset. The handler smoke test `test_lobby_runtime_reset_clears_local_shared_and_last_launch_state` protects that behavior.

## Mutation Surface That Is Not A Clear

Do not sweep these into the clear helper. They are publish/update semantics, not lifecycle cleanup:

- shared lobby metadata publish syncs, including `connect` and `server_id` updates.
- derived server id synchronization from local lobby to matching shared lobby.
- reconnect / launch / postgame restore reads that may inspect shared state before deciding whether to preserve or clear.

These paths need named publish or lifecycle helpers, not a broader clear helper.

## Safe Next Steps

1. Add focused tests before each new lifecycle clear helper.
2. Introduce named wrappers only when the name captures intent, for example:
   - `GBE_ClearSharedDotaLobbyForRuntimeReset()`
   - `GBE_ClearSharedDotaLobbyForPlayerPostgameCleanup()`
   - `GBE_ClearSharedDotaLobbyForNormalSignout()`
3. Keep each wrapper behavior-equivalent at first.
4. Only after behavior-equivalent wrappers are covered, consider lifecycle-specific preserve decisions.

## Stop Conditions

Stop immediately if a change:

- combines clear behavior with publish behavior.
- changes host-client/server-GC ownership behavior without a focused test.
- changes reconnect preservation behavior without a focused test.
- adds logging or side effects to the raw clear helper.
- makes review harder than direct call-site inspection.
