# Phase 3.1 Handler Inventory

This inventory records the current `Steam_Game_Coordinator::GBE_HandleDota*` functions in `dll/gbe_dota_handlers.cpp` and groups them by likely extraction domain. It is intended to guide low-risk file splits before any signature or dispatch-table refactor.

## Current State

- Source file: `dll/gbe_dota_handlers.cpp`
- Handler member functions: 62 originally; 3 inventory (Phase 3.1.2) + 7 chat/broadcast (Phase 3.1.3) + 17 lobby (Phase 3.1.4) + 10 direct 7034 match-flow + 3 custom-game loading (Phase 3.1.5a) + 17 misc one-off (Phase 3.1.5c) + 1 template-replay + 4 post-login/socket/server-assignment (Phase 3.1.5b) extracted, leaving 0 in `dll/gbe_dota_handlers.cpp` (now an 81-line include + using-aliases shell).
- Handler-local static symbols: 23 originally; 1 chat-only static + 6 lobby-only statics + 1 match-flow static + 1 handler-local struct (`GBE_ProtoField`) + 12 template statics + 2 session constants all moved with their respective handler groups. Zero handler-local statics remain in `dll/gbe_dota_handlers.cpp`.
- Primary risk: static helper/data placement when handlers are moved into domain-specific files.

## Direct / Post-Login / Protocol Handlers

These functions handle direct GC request routing, direct 7034 flow, generic direct replies, and template replay. They should remain together until the direct request dispatch path is better covered by tests.

### Direct 7034 Match-Flow Handlers

**EXTRACTED to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a).** The single match-flow static `GBE_AdaptDota7034ConnectedPlayersResponsePayload` moved with them (List X, kept `static` in new TU); no cross-TU externalization was needed (List Y = 0).

- `GBE_HandleDotaDirect7034Request` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirectOwnerHeroKnownEquipReplay` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034DisconnectedPlayers` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034RuntimeUpdates` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034StrategyTime` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034StrategyTimeFallback` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034StrategyTimePreserve` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034Response` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034LaunchPoll` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.
- `GBE_HandleDotaDirect7034WaitForPlayers` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.

### Misc One-Off Handlers

**EXTRACTED to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c).** No handler-local statics moved with this group (List X = 0, the handler-local struct `GBE_ProtoField` moved with the group and is kept file-local in the new TU); no cross-TU externalization was needed (List Y = 0, only the `GBE_DotaEmptyRequestShape` and `GBE_DotaRankRequestShape` aliases were repeated in the new TU).

- `GBE_HandleDotaMinimalVarintSuccessRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDota7427NotificationsRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaUploadRateRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaProfileCardRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaLookupAccountNameRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaEmoticonDataRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaConductScorecardRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaCoachingSummaryRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaRankRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaLaunchAdvanceOrConsume` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDota8870LaunchMarkerRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaLanServerAvailableRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaBatchPlayerResourcesRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaCacheSubscriptionRefreshRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaLeaverDetectedRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaSignOutPermissionRequest` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.
- `GBE_HandleDotaSubmitPlayerReportV2Request` — **moved to `dll/gbe_dota_misc_handlers.cpp` (Phase 3.1.5c)**.

### Remaining Post-Login / Socket / Template Handlers

**EXTRACTED (Phase 3.1.5b).** Split into two files because TemplateReplay (canned-response switch) is a different concern from post-login/socket/server-assignment (session establishment).

- `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_HandleDotaServerAssignmentRequest` — **moved to `dll/gbe_dota_post_login_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_HandleDotaDirectPostLoginRequest` — **moved to `dll/gbe_dota_post_login_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_HandleDotaAddSocketRequest` — **moved to `dll/gbe_dota_post_login_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_HandleDotaWrappedPostLoginRequest` — **moved to `dll/gbe_dota_post_login_handlers.cpp` (Phase 3.1.5b)**.

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

**MERGED into `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a).** These handlers were first extracted to a standalone `dll/gbe_dota_custom_game_handlers.cpp` (Phase 3.1.5), then merged into the match-flow file because they are a sub-phase of the 7034 launch flow (they share `GBE_local_lobby.launch_phase` and `GBE_TryAdvanceDotaLaunchToRun` with the 7034 path). The standalone 187-line file was below the 300-line healthy lower bound and was removed. No handler-local statics moved with this group (List X = 0); no cross-TU externalization was needed (List Y = 0, only the `GBE_Dota8053Result` alias was repeated in the new TU).

- `GBE_HandleDotaCustomGameReadyUpRequest` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a, via Phase 3.1.5)**.
- `GBE_HandleDotaCustomGameStartedLoadingRequest` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a, via Phase 3.1.5)**.
- `GBE_HandleDotaCustomGameFinishedLoadingRequest` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a, via Phase 3.1.5)**.

## Handler-Local Static Symbols

### Direct / Protocol Statics

- `GBE_kSteamGamesPlayedWithDataBlob`: used by `GBE_HandleDotaDirectPostLoginRequest` — **moved to `dll/gbe_dota_post_login_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kSteamAuthList`: used by `GBE_HandleDotaDirectPostLoginRequest` — **moved to `dll/gbe_dota_post_login_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota8678Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota8136Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota2538Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota2618Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota8674Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota8677Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota7198Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota8079Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota8854Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_kDota9024Template`: used by `GBE_HandleDotaTemplateReplayRequest` — **moved to `dll/gbe_dota_template_replay_handlers.cpp` (Phase 3.1.5b)**.
- `GBE_AdaptDota7034ConnectedPlayersResponsePayload`: used by `GBE_HandleDotaDirect7034Response` — **moved to `dll/gbe_dota_match_handlers.cpp` (Phase 3.1.5a)**.

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
