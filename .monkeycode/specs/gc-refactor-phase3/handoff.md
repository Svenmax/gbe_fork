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
- Phase 3.1.5 custom-game loading sub-batch is complete (match and misc sub-buckets still pending).
  - Added `dll/gbe_dota_custom_game_handlers.cpp` (187 lines).
  - Moved 3 custom-game loading handlers: `GBE_HandleDotaCustomGameReadyUpRequest` (7070), `GBE_HandleDotaCustomGameStartedLoadingRequest` (8052), `GBE_HandleDotaCustomGameFinishedLoadingRequest` (8053).
  - List X = 0 (no handler-local statics), List Y = 0 (no cross-TU externalization; only `GBE_Dota8053Result` alias repeated in new TU).
  - Note: 187 lines is below the 300-line healthy lower bound. Decision: keep as a temporary standalone file; when the match/7034 sub-bucket is extracted next, merge these 3 custom-game loading handlers into `dll/gbe_dota_match_handlers.cpp` (they are part of the match launch flow and share `GBE_local_lobby.launch_phase` / `GBE_TryAdvanceDotaLaunchToRun` with the 7034 path).
- Handler signatures and function bodies were kept unchanged.
- `dll/gbe_dota_handlers.cpp` is currently 3459 lines (down from 3578 after the 3.1.5 custom-game sub-batch).
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

- Continue mechanical extraction in Phase 3.1.5: the custom-game loading sub-batch is done, next sub-buckets are the match/7034 flow group and the misc one-off handlers; or start the follow-up logic refactor for an already-extracted bucket (3.1.7 inventory, 3.1.8 chat, 3.1.9 lobby, 3.1.10 custom-game/match/misc logic refactor).
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
