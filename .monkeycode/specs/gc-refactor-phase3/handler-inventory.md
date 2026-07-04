# Phase 3.1 Handler Inventory

This inventory records the current `Steam_Game_Coordinator::GBE_HandleDota*` functions in `dll/gbe_dota_handlers.cpp` and groups them by likely extraction domain. It is intended to guide low-risk file splits before any signature or dispatch-table refactor.

## Current State

- Source file: `dll/gbe_dota_handlers.cpp`
- Handler member functions: 62 originally; 3 inventory (Phase 3.1.2) + 7 chat/broadcast (Phase 3.1.3) + 17 lobby (Phase 3.1.4) extracted, leaving 35 in `dll/gbe_dota_handlers.cpp`.
- Handler-local static symbols: 23 originally; 1 chat-only static (`GBE_GenerateDotaChatChannelId`) moved with the chat handlers (Phase 3.1.3) + 6 lobby-only statics moved with the lobby handlers (Phase 3.1.4), leaving 15.
- Primary risk: static helper/data placement when handlers are moved into domain-specific files.

## Direct / Post-Login / Protocol Handlers

These functions handle direct GC request routing, direct 7034 flow, generic direct replies, and template replay. They should remain together until the direct request dispatch path is better covered by tests.

- `GBE_HandleDotaDirect7034Request`
- `GBE_HandleDotaDirectOwnerHeroKnownEquipReplay`
- `GBE_HandleDotaDirect7034DisconnectedPlayers`
- `GBE_HandleDotaDirect7034RuntimeUpdates`
- `GBE_HandleDotaDirect7034StrategyTime`
- `GBE_HandleDotaDirect7034StrategyTimeFallback`
- `GBE_HandleDotaDirect7034StrategyTimePreserve`
- `GBE_HandleDotaDirect7034Response`
- `GBE_HandleDotaDirect7034LaunchPoll`
- `GBE_HandleDotaDirect7034WaitForPlayers`
- `GBE_HandleDotaMinimalVarintSuccessRequest`
- `GBE_HandleDota7427NotificationsRequest`
- `GBE_HandleDotaUploadRateRequest`
- `GBE_HandleDotaProfileCardRequest`
- `GBE_HandleDotaLookupAccountNameRequest`
- `GBE_HandleDotaEmoticonDataRequest`
- `GBE_HandleDotaConductScorecardRequest`
- `GBE_HandleDotaCoachingSummaryRequest`
- `GBE_HandleDotaRankRequest`
- `GBE_HandleDotaLaunchAdvanceOrConsume`
- `GBE_HandleDota8870LaunchMarkerRequest`
- `GBE_HandleDotaLanServerAvailableRequest`
- `GBE_HandleDotaBatchPlayerResourcesRequest`
- `GBE_HandleDotaCacheSubscriptionRefreshRequest`
- `GBE_HandleDotaLeaverDetectedRequest`
- `GBE_HandleDotaSignOutPermissionRequest`
- `GBE_HandleDotaSubmitPlayerReportV2Request`
- `GBE_HandleDotaTemplateReplayRequest`
- `GBE_HandleDotaServerAssignmentRequest`
- `GBE_HandleDotaDirectPostLoginRequest`
- `GBE_HandleDotaAddSocketRequest`
- `GBE_HandleDotaWrappedPostLoginRequest`

## Inventory / Item Handlers

These are the best first extraction candidates because their domain is narrow and some helper behavior is already covered by payload helper tests.

- `GBE_HandleDotaUnlockItemStyleRequest`
- `GBE_HandleDotaSetItemStyleRequest`
- `GBE_HandleDotaEquipItemsRequest`

## Lobby Handlers

**EXTRACTED to `dll/gbe_dota_lobby_handlers.cpp` (Phase 3.1.4).** Six lobby-only statics (`GBE_ApplyDotaCustomGameDetailsRequest`, `GBE_NormalizeDotaCustomGameDetailsFromInstalledMod`, `GBE_GenerateDotaLobbyId`, `GBE_GenerateDotaMatchId`, `GBE_AdaptDotaLobbyInviteCacheSubscribedPayload`, `GBE_IsDotaLobbyInviteCacheSubscribedPayload`) moved with them; no cross-TU externalization was needed (List Y = 0).

These functions form the largest coherent business group. Extract them after inventory handlers because they share more local helpers and mutable lobby state.

- `GBE_HandleDotaPracticeLobbyCreateRequest`
- `GBE_HandleDotaLobbyListRequest`
- `GBE_HandleDotaCustomLobbyListRequest`
- `GBE_HandleDotaFriendPracticeLobbyListRequest`
- `GBE_HandleDotaPracticeLobbyJoinRequest`
- `GBE_HandleDotaInviteToLobbyRequest`
- `GBE_HandleDotaLobbyInviteResponseRequest`
- `GBE_HandleDotaFriendLobbyInviteMessage`
- `GBE_HandleDotaNetworkLobbyInviteMessage`
- `GBE_HandleDotaAbandonCurrentGameRequest`
- `GBE_HandleDotaGameMatchSignOutRequest`
- `GBE_HandleDotaPracticeLobbyLeaveRequest`
- `GBE_HandleDotaPracticeLobbyLaunchRequest`
- `GBE_HandleDotaPracticeLobbySetDetailsRequest`
- `GBE_HandleDotaPracticeLobbySetTeamSlotRequest`
- `GBE_HandleDotaPracticeLobbyKickRequest`
- `GBE_HandleDotaDestroyLobbyRequest`

## Chat / Broadcast Handlers

**EXTRACTED to `dll/gbe_dota_chat_handlers.cpp` (Phase 3.1.3).** The chat-only static `GBE_GenerateDotaChatChannelId` moved with them; no cross-TU externalization was needed (List Y = 0).

- `GBE_HandleDotaJoinChatChannelRequest`
- `GBE_HandleDotaChatMessageRequest`
- `GBE_HandleDotaNetworkChatMessage`
- `GBE_HandleDotaLeaveChatChannelRequest`
- `GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest`
- `GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest`
- `GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest`

## Custom Game Loading Handlers

These are compact and can move into either a custom-game handler file or the direct/protocol group depending on future dispatch-table shape.

- `GBE_HandleDotaCustomGameReadyUpRequest`
- `GBE_HandleDotaCustomGameStartedLoadingRequest`
- `GBE_HandleDotaCustomGameFinishedLoadingRequest`

## Handler-Local Static Symbols

### Direct / Protocol Statics

- `GBE_kSteamGamesPlayedWithDataBlob`: used by `GBE_HandleDotaDirectPostLoginRequest`
- `GBE_kSteamAuthList`: used by `GBE_HandleDotaDirectPostLoginRequest`
- `GBE_kDota8678Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota8136Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota2538Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota2618Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota8674Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota8677Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota7198Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota8079Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota8854Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_kDota9024Template`: used by `GBE_HandleDotaTemplateReplayRequest`
- `GBE_AdaptDota7034ConnectedPlayersResponsePayload`: used by `GBE_HandleDotaDirect7034Response`

### Lobby Statics

- `GBE_ApplyDotaCustomGameDetailsRequest`: used by `GBE_HandleDotaPracticeLobbyCreateRequest` and `GBE_HandleDotaPracticeLobbySetDetailsRequest` — **moved to `dll/gbe_dota_lobby_handlers.cpp` (Phase 3.1.4)**.
- `GBE_NormalizeDotaCustomGameDetailsFromInstalledMod`: used by `GBE_HandleDotaPracticeLobbyCreateRequest` and `GBE_HandleDotaPracticeLobbySetDetailsRequest` — **moved to `dll/gbe_dota_lobby_handlers.cpp` (Phase 3.1.4)**.
- `GBE_GenerateDotaLobbyId`: used by `GBE_HandleDotaPracticeLobbyCreateRequest` and `GBE_HandleDotaPracticeLobbyJoinRequest` — **moved to `dll/gbe_dota_lobby_handlers.cpp` (Phase 3.1.4)**.
- `GBE_GenerateDotaMatchId`: used by `GBE_HandleDotaPracticeLobbyLaunchRequest` — **moved to `dll/gbe_dota_lobby_handlers.cpp` (Phase 3.1.4)**.
- `GBE_AdaptDotaLobbyInviteCacheSubscribedPayload`: used by `GBE_HandleDotaFriendLobbyInviteMessage` — **moved to `dll/gbe_dota_lobby_handlers.cpp` (Phase 3.1.4)**.
- `GBE_IsDotaLobbyInviteCacheSubscribedPayload`: used by `GBE_HandleDotaNetworkLobbyInviteMessage` — **moved to `dll/gbe_dota_lobby_handlers.cpp` (Phase 3.1.4)**.

### Chat Statics

- `GBE_GenerateDotaChatChannelId`: used by `GBE_HandleDotaJoinChatChannelRequest` — **moved to `dll/gbe_dota_chat_handlers.cpp` (Phase 3.1.3)**.

### Currently Unreferenced Static Templates

- `GBE_kDota8730TemplateHex`
- `GBE_kDota8331TemplateHex`
- `GBE_kDotaOfficial8745TemplateHex`

These should be verified before deletion because they may be fixtures retained for future protocol compatibility.

## Recommended First Extraction

Start with `dll/gbe_dota_inventory_handlers.cpp` and move only:

- `GBE_HandleDotaUnlockItemStyleRequest`
- `GBE_HandleDotaSetItemStyleRequest`
- `GBE_HandleDotaEquipItemsRequest`

Reasons:

- The group is small.
- It does not own the handler-local static templates listed above.
- It relies on helper functions that are already declared outside the handler file.
- Existing payload helper tests cover the most error-prone inventory helper behavior.

## Verification For Each Extraction Commit

- Run `python3 tools/_audit_gc_refactor.py`.
- Run `tools/run_gc_offline_tests.sh`.
- Confirm `dll/gbe_dota_handlers.cpp` line count decreases.
- Confirm no new cross-TU helper is added to `gbe_dota_gc_internal.h` unless unavoidable.
