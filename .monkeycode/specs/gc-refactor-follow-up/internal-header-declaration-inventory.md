# GC Internal Header Declaration Inventory

## Scope

This inventory classifies the remaining declarations in `dll/gbe_dota_gc_internal.h` after the first follow-up pass. It is a planning document for Stage 1 header shrink work. It records which declarations should stay temporarily, which declarations are candidates for narrow headers, and which declarations can later move down into a defining translation unit.

## Current Narrow Header Baseline

- `dll/gbe_dota_payload_item_helpers.h` already owns item payload declarations for equip op parsing, style bitmask mutation, SO object construction, and item serialization.
- `dll/gbe_dota_gc_internal.h` still carries mixed declarations for logging, wire payload, lobby payload, template replay, shared mutable state, server/client hello context, and stateful orchestration helpers.

## Logging And Trace

Keep in `gbe_dota_gc_internal.h` for now:

- `GBE_GC_DebugLog`
- `GBE_DescribeDotaLaunchPhase`
- `GBE_LogDotaSOCacheSubscribedSummary`
- `GBE_LogDotaResponsePacket`
- `GBE_LogGCProtoBoundary`

Reason: these are cross-cutting diagnostics used by multiple production TUs and tests. A future `gbe_dota_gc_logging.h` would be a clean narrow owner, but moving logging alone is low risk and can be handled after the payload declarations move.

## Wire Payload And Proto Patch

Migrate in Stage 1.2 to a narrow wire header, suggested name `dll/gbe_dota_payload_wire_helpers.h`:

- `GBE_RewriteAccountIdVarintInDirectProtoBody`
- `GBE_TryPatchDotaAccountIdVarint`
- `GBE_TryPatchDotaAccountIdFixed32`
- `GBE_PatchDotaTemplateIdentifiers`
- `GBE_PrepareDotaDirectReplayMessage`
- `GBE_ForceDotaLobbyUpdateOwnerSOID`
- `GBE_PatchDotaPracticeLobbyLaunchTemplate`
- `GBE_PatchDotaLobbyTemplateIdentifiers`
- `GBE_PatchDotaLobbyTemplateIdentifiersIfPresent`
- `GBE_ForceDotaLobbyCacheOwnerSOID`
- `GBE_PrepareDotaWelcomeBody`
- `GBE_ExtractDotaHelloContext`
- `GBE_ExtractDirectDotaHelloContext`
- `GBE_ExtractDirectDotaServerHelloContext`
- `GBE_BuildDirectDotaClientWelcome`
- `GBE_ComposeDotaClientWelcome`

Keep with the wire header initially:

- `GBE_DotaHelloContext`
- `GBE_DotaServerHelloContext`
- `GBE_BuildDirectDotaServerWelcome`
- `GBE_HasLastDotaServerHelloContext`
- `GBE_GetLastDotaServerHelloContext`
- `GBE_SetLastDotaServerHelloContext`
- `GBE_ClearLastDotaServerHelloContext`

Reason: the hello context structs and welcome builders are wire-facing, but the server-hello cache accessors are stateful. Keeping them together for Stage 1.2 minimizes include churn; a later state header can split the cache accessors once direct/welcome ownership is clearer.

## Lobby Payload And Practice Lobby Builders

Migrate in Stage 1.3 to a narrow lobby payload header, suggested name `dll/gbe_dota_payload_lobby_helpers.h`:

- `GBE_AdaptDotaJoinChatChannelResponsePayload`
- `GBE_DotaCustomGameDisplayName`
- `GBE_ReplayDotaPracticeLobbyOfficial26Payload`
- `GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate`
- `GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload`
- `GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl`
- `GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl`
- `GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState`
- `GBE_ComposeDotaPracticeLobbySOObjects`
- `GBE_AdaptDotaTopCustomGamesListPayload`
- `GBE_IsDotaOtherLeftChannelPayloadForChannel`

Reason: these declarations have long lobby-shaped parameter lists and are used by lobby snapshot, lobby state, post-login, chat, and replay paths. Moving them together creates a useful boundary without mixing in item payload helpers.

## Template And Replay Ownership

Stage 1.4 decision: keep in `gbe_dota_gc_internal.h` for now:

- `GBE_kDotaAbandonPersonaStateInitHex`
- `GBE_kDotaOfficial032PracticeLobby26Hex`
- `GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex`
- `GBE_kDotaPracticeLobbyCacheSubscribedTemplate`
- `GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage`
- `GBE_PrepareDotaPersonaStatePeripheralMessage`

Reason: these are tightly coupled to canned template/replay ownership and are still shared across chat abandon, launch flow, launch fallback, lobby snapshot replay, and lobby payload builder paths. Existing audit coverage requires large canned replay data to stay in template replay or payload helper owners. Moving these now would require a template/replay public header before the remaining ownership is narrower than `gbe_dota_gc_internal.h`.

Revisit condition: create a dedicated template/replay header only after either all persona peripheral call sites route through a launch/chat seam or all cache-subscribed/official 26 replay call sites route through `gbe_dota_payload_lobby_helpers.h` without needing direct constant access.

## Shared Tables And Serializer Utilities

Keep in `gbe_dota_gc_internal.h` for now:

- `ser_var`
- `deser_var`
- `GBE_kOldDotaAccountIdVarint`
- `GBE_kOldDotaSteamIdVarint`
- `GBE_kOldDotaLobbyIdVarint`
- `GBE_kOldDotaSteamIdFixed64`
- `GBE_kOldDotaPersonaSteamIdFixed64`
- `GBE_kOldDotaAccountIdFixed32`
- `GBE_kOldDotaPracticeLobbyMatchIdVarint`
- `GBE_kOldDotaPracticeLobbyServerIdFixed64`
- `GBE_kOldDotaPracticeLobbyLobbyIdText`
- `GBE_kOldDotaPracticeLobbyLobbyIdTextAlt`
- `GBE_kSteamTicketAuthComplete`

Reason: these tables are still shared by multiple patch/template helpers. Moving them before the wire/lobby split would create more include churn than value.

## Shared Mutable State And Stateful Orchestration

Keep in `gbe_dota_gc_internal.h` until dedicated state ownership exists:

- `GBE_shared_dota_lobby_state`
- `GBE_vpk_loot_data`
- `GBE_last_dota_server_hello_context`
- `GBE_PushDotaPlayerEquippedItemsCacheToGC`

Reason: these declarations expose stateful coordinator behavior and side effects. They should move only after dependency seams or state accessors give them a narrower owner.

## Down-Sink Candidates

Consider moving down to the defining TU or marking file-local after caller analysis:

- `GBE_last_dota_server_hello_context`: retain storage in `steam_game_coordinator.cpp`; callers should prefer accessors.
- `GBE_kDotaPracticeLobbyCacheSubscribedTemplate`: keep near its patch/build owner if all non-owner references disappear.
- `GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex`: keep near template replay owner if all cross-TU uses are removed.
- `GBE_kDotaOfficial032PracticeLobby26Hex`: keep near official 26 replay owner if all cross-TU uses are removed.

## Recommended Next Moves

1. Stage 1.2: create `dll/gbe_dota_payload_wire_helpers.h` and migrate only wire/hello declarations.
2. Stage 1.3: create `dll/gbe_dota_payload_lobby_helpers.h` and migrate lobby payload declarations.
3. Stage 1.4: revisit template/replay constants and peripheral builders after the two narrow headers reduce include pressure.
4. After Stage 1.4: evaluate a small `dll/gbe_dota_gc_logging.h` and a state/accessor header if cross-TU logging or state includes still dominate `gbe_dota_gc_internal.h`.
