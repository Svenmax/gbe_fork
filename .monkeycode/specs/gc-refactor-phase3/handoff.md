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
- Phase 3.1.5a match-flow sub-batch is complete (post-login and misc sub-buckets still pending).
  - Added `dll/gbe_dota_match_handlers.cpp` (802 lines).
  - Moved 10 direct 7034 match-flow handlers: `GBE_HandleDotaDirect7034Request`, `GBE_HandleDotaDirectOwnerHeroKnownEquipReplay`, `GBE_HandleDotaDirect7034DisconnectedPlayers`, `GBE_HandleDotaDirect7034RuntimeUpdates`, `GBE_HandleDotaDirect7034StrategyTime`, `GBE_HandleDotaDirect7034StrategyTimeFallback`, `GBE_HandleDotaDirect7034StrategyTimePreserve`, `GBE_HandleDotaDirect7034Response`, `GBE_HandleDotaDirect7034LaunchPoll`, `GBE_HandleDotaDirect7034WaitForPlayers`.
  - Moved 1 static `GBE_AdaptDota7034ConnectedPlayersResponsePayload` (List X, kept `static` in new TU).
  - Merged in the 3 custom-game loading handlers (`GBE_HandleDotaCustomGameReadyUpRequest` 7070, `GBE_HandleDotaCustomGameStartedLoadingRequest` 8052, `GBE_HandleDotaCustomGameFinishedLoadingRequest` 8053) that were previously extracted to `dll/gbe_dota_custom_game_handlers.cpp`; that standalone file was removed because custom-game loading is a sub-phase of the 7034 launch flow, not an independent domain (the prior 187-line file was below the 300-line healthy lower bound).
  - List Y = 0 (no cross-TU externalization; 4 `using` aliases repeated in new TU: `GBE_Dota7034RequestShape`, `GBE_Dota7034ConnectedPlayer`, `GBE_Dota7034DisconnectedPlayer`, `GBE_Dota8053Result`).
- Handler signatures and function bodies were kept unchanged.
- `dll/gbe_dota_handlers.cpp` is currently 2857 lines (down from 3459 after the 3.1.5a match sub-batch).
- `premake5.lua` uses `dll/**` in `common_files`, so the new cpp file is included by the main build source glob.

## Verification

- `python3 tools/_audit_gc_refactor.py` passed.
- `tools/run_gc_offline_tests.sh` passed.
- Payload helper test result: `92/92 passed`.
- Audit result:
  - Zombie declarations: `0`
  - Under-exposed definitions: `0`
  - Doc line-number mismatches: `0`

## Recommended Next Step

- Continue mechanical extraction in Phase 3.1.5: the 3.1.5a match-flow sub-batch is done, next sub-buckets are 3.1.5b (post-login/socket/template-replay handlers + their static templates) and 3.1.5c (misc one-off handlers); or start the follow-up logic refactor for an already-extracted bucket (3.1.7 inventory, 3.1.8 chat, 3.1.9 lobby, 3.1.10 match/misc logic refactor).
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
