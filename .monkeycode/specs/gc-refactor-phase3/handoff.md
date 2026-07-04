# GC Refactor Phase 3 Handoff

## Current Context

- Project path: `/workspace`
- Current branch: `trae/agent-inRF11`

## Completed Work

- Phase 3.0 audit stabilization is complete.
- Phase 3.1.1 handler inventory is complete.
- Phase 3.1.2 inventory handler extraction is complete.
  - Added `dll/gbe_dota_inventory_handlers.cpp` (513 lines).
  - Moved `GBE_HandleDotaUnlockItemStyleRequest`, `GBE_HandleDotaSetItemStyleRequest`, `GBE_HandleDotaEquipItemsRequest`.
- Phase 3.1.3 chat handler extraction is complete.
  - Added `dll/gbe_dota_chat_handlers.cpp` (653 lines).
  - Moved 7 chat/broadcast handlers: `GBE_HandleDotaJoinChatChannelRequest`, `GBE_HandleDotaChatMessageRequest`, `GBE_HandleDotaNetworkChatMessage`, `GBE_HandleDotaLeaveChatChannelRequest`, `GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest`, `GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest`, `GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest`.
  - Moved chat-only static `GBE_GenerateDotaChatChannelId` (List X, kept `static` in new TU).
  - List Y = 0: no cross-TU externalization needed; all shared symbols already declared in existing headers.
- Phase 3.1.4 lobby handler extraction is complete.
  - Added `dll/gbe_dota_lobby_handlers.cpp` (1663 lines).
  - Moved 17 lobby handlers covering create/list/join/invite/invite-response/friend-invite/network-invite/abandon/signout/leave/launch/set-details/set-team-slot/kick/destroy.
  - Moved 6 lobby-only statics (`GBE_ApplyDotaCustomGameDetailsRequest`, `GBE_NormalizeDotaCustomGameDetailsFromInstalledMod`, `GBE_GenerateDotaLobbyId`, `GBE_GenerateDotaMatchId`, `GBE_AdaptDotaLobbyInviteCacheSubscribedPayload`, `GBE_IsDotaLobbyInviteCacheSubscribedPayload`), all kept `static` in new TU (List X).
  - List Y = 0: no cross-TU externalization needed; all shared symbols already declared in existing headers.
- Phase 3.1.5a match-flow sub-batch is complete (post-login sub-bucket still pending, misc sub-bucket done).
  - Added `dll/gbe_dota_match_handlers.cpp` (802 lines).
  - Moved 10 direct 7034 match-flow handlers: `GBE_HandleDotaDirect7034Request`, `GBE_HandleDotaDirectOwnerHeroKnownEquipReplay`, `GBE_HandleDotaDirect7034DisconnectedPlayers`, `GBE_HandleDotaDirect7034RuntimeUpdates`, `GBE_HandleDotaDirect7034StrategyTime`, `GBE_HandleDotaDirect7034StrategyTimeFallback`, `GBE_HandleDotaDirect7034StrategyTimePreserve`, `GBE_HandleDotaDirect7034Response`, `GBE_HandleDotaDirect7034LaunchPoll`, `GBE_HandleDotaDirect7034WaitForPlayers`.
  - Moved 1 static `GBE_AdaptDota7034ConnectedPlayersResponsePayload` (List X, kept `static` in new TU).
  - Merged in the 3 custom-game loading handlers (`GBE_HandleDotaCustomGameReadyUpRequest` 7070, `GBE_HandleDotaCustomGameStartedLoadingRequest` 8052, `GBE_HandleDotaCustomGameFinishedLoadingRequest` 8053) that were previously extracted to `dll/gbe_dota_custom_game_handlers.cpp`; that standalone file was removed because custom-game loading is a sub-phase of the 7034 launch flow, not an independent domain (the prior 187-line file was below the 300-line healthy lower bound).
  - List Y = 0 (no cross-TU externalization; 4 `using` aliases repeated in new TU: `GBE_Dota7034RequestShape`, `GBE_Dota7034ConnectedPlayer`, `GBE_Dota7034DisconnectedPlayer`, `GBE_Dota8053Result`).
- Phase 3.1.5c misc sub-batch is complete (post-login sub-bucket still pending).
  - Added `dll/gbe_dota_misc_handlers.cpp` (726 lines).
  - Moved 17 misc one-off handlers: `GBE_HandleDotaMinimalVarintSuccessRequest`, `GBE_HandleDota7427NotificationsRequest`, `GBE_HandleDotaUploadRateRequest`, `GBE_HandleDotaProfileCardRequest`, `GBE_HandleDotaLookupAccountNameRequest`, `GBE_HandleDotaEmoticonDataRequest`, `GBE_HandleDotaConductScorecardRequest`, `GBE_HandleDotaCoachingSummaryRequest`, `GBE_HandleDotaRankRequest`, `GBE_HandleDotaLaunchAdvanceOrConsume`, `GBE_HandleDota8870LaunchMarkerRequest`, `GBE_HandleDotaLanServerAvailableRequest`, `GBE_HandleDotaBatchPlayerResourcesRequest`, `GBE_HandleDotaCacheSubscriptionRefreshRequest`, `GBE_HandleDotaLeaverDetectedRequest`, `GBE_HandleDotaSignOutPermissionRequest`, `GBE_HandleDotaSubmitPlayerReportV2Request`.
  - Moved 1 handler-local struct `GBE_ProtoField` (List X, kept file-local in new TU; only used by `GBE_HandleDotaBatchPlayerResourcesRequest`).
  - List Y = 0 (no cross-TU externalization; 2 `using` aliases repeated in new TU: `GBE_DotaEmptyRequestShape`, `GBE_DotaRankRequestShape`).
- Phase 3.1.5b post-login/template sub-batch is complete. All mechanical extraction in Phase 3.1.5 is done.
  - Added `dll/gbe_dota_template_replay_handlers.cpp` (1255 lines).
  - Moved 1 handler `GBE_HandleDotaTemplateReplayRequest` + 12 template statics (10 `GBE_kDota*Template[]` byte arrays + 3 `GBE_kDota*TemplateHex` hex strings), all kept `static` in new TU (List X). The single handler is 1082 lines because it is a large canned-response switch.
  - Added `dll/gbe_dota_post_login_handlers.cpp` (1011 lines).
  - Moved 4 handlers (`GBE_HandleDotaServerAssignmentRequest`, `GBE_HandleDotaDirectPostLoginRequest`, `GBE_HandleDotaAddSocketRequest`, `GBE_HandleDotaWrappedPostLoginRequest`) + 2 session constants (`GBE_kSteamGamesPlayedWithDataBlob`, `GBE_kSteamAuthList`), all kept `static` in new TU (List X).
  - Split into two files because TemplateReplay (canned-response switch) is a different concern from post-login/socket/server-assignment (session establishment); keeping them together would have produced a 2193-line file exceeding the 2000-line tolerance.
  - List Y = 0 for both files (all 5 handlers already declared in `dll/dll/steam_game_coordinator.h`).
- Handler signatures and function bodies were kept unchanged.
- `dll/gbe_dota_handlers.cpp` has been **removed entirely** (Phase 3.1.6 complete). The original 5757-line monolith is fully decomposed into 7 domain files: inventory (513), chat (653), lobby (1663), match (802), misc (726), template_replay (1255), post_login (1011). Total ~6623 lines across 7 files. All 62 handlers extracted, zero handler-local statics remain orphaned.
- `premake5.lua` uses `dll/**` in `common_files`, so the new cpp file is included by the main build source glob.
- Phase 3.1.6.5 handler-level test harness is complete.
  - Created `tools/gbe_dota_handler_test/` with 4 files:
    - `stubs.h` — recording `Steam_Game_Coordinator` stub class + `ActionRecorder` (7 action types: `PushIncomingNow`, `PushIncoming`, `SaveItemsToFile`, `CallbackItemUpdated`, `ServerGcForward`, `NetworkBroadcast`, `LobbySnapshotRefresh`) + Steam SDK type stubs (`CSteamID`, `Settings`, `Networking`, `Econ_Item`, `Mod_entry`, `Steam_Client`, `GameServer_Items_Messages`, `Common_Message`) + `ESOMsg` enum + handler declarations for inventory domain.
    - `test_wrapper.cpp` — include-guard override pattern that pre-defines `__INCLUDED_STEAM_GAME_COORDINATOR_H__` etc. + protobuf guards, then includes `stubs.h` and `dll/gbe_dota_inventory_handlers.cpp` inline.
    - `free_func_stubs.cpp` — stubs for `GBE_GC_DebugLog`, `get_steam_client()`, `GBE_PushDotaPlayerEquippedItemsCacheToGC`, `GBE_ApplyDotaUnlockStyleBitmask`, `GBE_ParseDotaEquipOps`, `get_full_program_path()`.
    - `smoke_test.cpp` — 6 inventory domain smoke tests (unlock style with/without consumable, item not found, invalid index, set style success, set style item not found) with varint encoding helpers + `TestFixture` (RAII setup of coordinator/settings/network/recorder).
  - Extended `tools/gbe_dota_gc_payload_helpers_test/pb_stubs/tf2/base_gcmessages.pb.h` with backward-compatible method additions on `CMsgSOMultipleObjects` (`add_objects()`, `set_version()`, `set_service_id()`) and `CMsgSOMultipleObjects_Object` (`set_type_id()`, `set_object_data(const std::string&)`).
  - Updated `tools/run_gc_offline_tests.sh` to build and run `gbe_dota_handler_test`.
  - Architecture: handlers are private member functions of `Steam_Game_Coordinator` with deep coupling to coordinator state. The include-guard override + recording stub class approach allows real handler code to compile and execute in an offline test environment without the full Steam SDK. The `ActionRecorder` captures side effects in order for sequence assertions.
  - Extension pattern for future domains (3.1.8 chat / 3.1.9 lobby / 3.1.10 match): each domain needs its own `test_wrapper.cpp` (different handler .cpp compiled inline via include-guard override), extended stub surface in `stubs.h` (more handler declarations + more coordinator member stubs), and smoke tests in `smoke_test.cpp`. The existing `stubs.h` already documents this pattern in comments.

## Verification

- `python3 tools/_audit_gc_refactor.py` passed.
- `tools/run_gc_offline_tests.sh` passed.
- Payload helper test result: `92/92 passed`.
- Handler test result: `6/6 passed` (inventory domain smoke tests).
- Full offline suite: `98/98 passed` (92 payload + 6 handler).
- Audit result:
  - Zombie declarations: `0`
  - Under-exposed definitions: `0`
  - Doc line-number mismatches: `0`

## Recommended Next Step

- Phase 3.1.5 mechanical extraction is fully complete (3.1.5a match + 3.1.5b post-login/template + 3.1.5c misc). All 62 handlers now live in 7 domain-specific files.
- Phase 3.1.6 complete: `dll/gbe_dota_handlers.cpp` removed entirely (was an 81-line shell with zero definitions).
- Phase 3.1.6.5 complete: handler-level test harness built with 6/6 inventory smoke tests passing. Chat/lobby/match domain smoke tests deferred to their respective logic-refactor tasks (3.1.8/3.1.9/3.1.10) where the stub surface will be extended.
- Next step is 3.1.6.6 (side-effect action model) as the final prerequisite before logic refactors 3.1.7-3.1.10. The action model defines the contract between pure helpers and coordinator methods; defining it once (before any logic refactor) ensures cross-domain consistency.
- **Plan revisions (2026-07-04)**: Phase 3 plan was reviewed end-to-end. Key changes: (1) handler-file target relaxed from 1000 to 1500 lines because post-login/socket/template handlers share protocol state that resists splitting before 3.3; (2) added task 3.1.6.5 "Build handler-level test harness" as a prerequisite for 3.1.7-3.1.10, because the existing 92 offline tests are payload-helper-level only and provide zero handler-behavior coverage; (3) moved the side-effect action model (was 3.1.12) ahead to 3.1.6.6 so the action contract is defined once before any logic refactor; (4) added a boundary note to 3.1.9 clarifying that per-handler restructuring stays in 3.1.9 while cross-handler state-machine consolidation belongs to 3.4; (5) added a "good enough" stop condition to Success Metrics so the last 20% of file-size reduction does not drive unjustified abstractions. Remaining gaps (payload helper call-graph analysis, dispatch-table signature normalization risk, domain-specific line-count calibration) will be addressed at the start of 3.2 / 3.3 respectively.
- Treat mechanical extraction as an intermediate step only. Every GC domain moved into a new file must receive a follow-up logic refactor before that domain is considered complete.
- For each extracted domain, separate request parsing, state mutation, message/response construction, coordinator-owned side effects, and focused tests.
- Before changing handler internals, document the current side-effect order and preserve it with action-order tests or an explicit ordered action list.
- Pure helper code should build decisions, payloads, mutations, or action lists. Coordinator methods should execute real side effects serially.
- Preserve protocol-sensitive order such as `CacheSubscribed` before `emsg21/26`, SO update before response, and full item cache before create/update.
- Keep file splits domain-cohesive. Prefer local helpers inside the domain `.cpp`; create new helper files for cross-domain reuse, pure testable logic, or size pressure.
- Treat handler buckets around `300-1200` lines and pure helpers around `200-1000` lines as healthy when responsibilities remain clear.
- Require a documented responsibility boundary and migration reason for every new GC `.cpp` file.
- Pick the next small group of low-coupling handlers from `handler-inventory.md`.
- Prefer handlers without handler-local static dependencies.
- Move 2 to 5 handlers per batch.
- Keep signatures, function bodies, and router call paths stable.
- After each batch, run:

```bash
python3 tools/_audit_gc_refactor.py
tools/run_gc_offline_tests.sh
```

- Before making new changes, inspect:

```bash
git status --short
```
