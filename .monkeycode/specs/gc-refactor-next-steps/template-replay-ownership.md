# Template And Replay Ownership

This document records the ownership boundaries for Dota GC canned bytes, replay templates, and patch points.

## Ownership Map

| Owner | Data | Responsibility |
| --- | --- | --- |
| `dll/gbe_dota_template_replay_handlers.cpp` | Direct legacy request replay templates for `2538`, `2618`, `7198`, `8136`, `8331`, `8674`, `8677`, `8678`, `8730`, `8745`, `8079`, `8854`, `9024` | Owns the request-to-template switch table and direct replay response orchestration for legacy emsgs. Templates remain file-local because they have one handler owner. |
| `dll/gbe_dota_gc_payload_helpers.cpp` | Practice lobby cache subscribed template bytes, launch cache subscribed official hex, abandon persona state init hex, official practice lobby 26 hex | Owns reusable payload/template composition and patch helpers shared by lobby, chat, flow, and replay paths. |
| `dll/gbe_dota_lobby_flow_coordinator.cpp` | Launch persona state hex variants for server setup, finding match, run, private lobby run | Owns launch rich presence/persona replay selection tied to launch phase. |
| `dll/gbe_dota_lobby_launch_coordinator.cpp` | Private lobby no-lobby abandon persona hex | Owns private lobby launch fallback persona replay for no-lobby teardown. |

## Template Categories

### Client Welcome

Client welcome payloads are constructed by payload helpers and should remain behind helper calls. Handler code should call the helper and avoid embedding welcome bytes directly.

### Server Welcome

Server welcome payloads are constructed by payload helpers and replay tests. Keep server welcome byte composition in helper/test utility code so request handlers only enqueue the result.

### Practice Lobby Cache Subscribed

Practice lobby cache subscribed templates live in `gbe_dota_gc_payload_helpers.cpp`. Patch points include lobby id, owner SteamID/SOID, match id, server id, game start time, connect endpoint, lobby state, game state, owner name, team/slot, and member runtime state.

### Persona State

Persona state hex templates are owned by the coordinator domain that selects them:

- Launch phase persona templates stay in `gbe_dota_lobby_flow_coordinator.cpp`.
- Abandon/no-lobby persona templates stay in the abandon or launch coordinator path that owns the branch.
- Shared persona patching should go through `GBE_PrepareDotaPersonaStatePeripheralMessage`.

### Official 26 Replay

Official practice lobby emsg 26 replay bytes live in `gbe_dota_gc_payload_helpers.cpp`. Replayers should call payload helper functions rather than embed official 26 hex in handlers.

## Patch Point Rules

When adding or modifying a template patch, document the patched fields near the helper or in tests. The patch point list should name the protocol value, the source of the replacement value, and whether the patch is required.

Required patch points should fail helper construction when missing. Optional patch points should return match counts so tests can assert whether the template actually contained the field.

## New Template Rule

New large canned bytes should be added to the narrowest owner:

- Single direct replay emsg: `gbe_dota_template_replay_handlers.cpp`
- Shared payload composition: `gbe_dota_gc_payload_helpers.cpp`
- Launch/persona state selection: launch or flow coordinator domain

Handlers should call helper functions and should not carry new large hex blobs or byte arrays when a helper boundary already exists.
