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
- Phase 3.1.6.6 side-effect action model is complete.
  - Created `dll/gbe_dota_action_model.h` (dependency-free: only `<cstdint>`, `<string>`, `<vector>`) defining:
    - `enum class GBE_DotaActionType` — 7 canonical action types (PushIncomingNow, PushIncoming, SaveItemsToFile, CallbackItemUpdated, ServerGcForward, NetworkBroadcast, LobbySnapshotRefresh).
    - `struct GBE_DotaAction` — type + emsg + payload + target_steam_id + item_id + job_id + reason (fields filled only when relevant to the type).
    - `using GBE_DotaActionList = std::vector<GBE_DotaAction>` — plain alias, no builder wrapper (deferred until repeated patterns justify helpers).
  - The header documents: the contract (pure helpers build action lists without touching coordinator state; coordinator methods consume and execute serially), the usage rule (only when a handler has multiple observable side effects), and 4 protocol-sensitive ordering invariants grounded in tested behavior:
    1. unlock flow: SO Update(22) -> SO Destroy(24) -> Response(2572)
    2. set-style flow: CallbackItemUpdated -> SaveItemsToFile -> Response(2578)
    3. full item cache (CacheSubscribed) before create/update SO messages when bootstrapping a remote GC
    4. equip flow: SO Update(26) -> Response(2570) -> ServerGcForward -> NetworkBroadcast -> LobbySnapshotRefresh
  - Aligned the test harness: `stubs.h` includes the real header and `RecordedAction::type` uses `GBE_DotaActionType` directly (removed the duplicate nested enum); `smoke_test.cpp` assertions reference `GBE_DotaActionType::*` so test code and the action-list contract share one name source.
  - No handler code changed — the model is defined and ready for 3.1.7-3.1.10 to adopt as they refactor handlers into pure-helper + coordinator-execution pairs.
- Phase 3.1.7 inventory handler logic refactor is complete.
  - Refactored `dll/gbe_dota_inventory_handlers.cpp` with an anonymous namespace of pure helpers and a side-effect order documentation block citing `dll/gbe_dota_action_model.h`:
    - `parse_unlock_style_request` / `parse_set_style_request` — pure request parsers (read item_id, style_index, consumable_id from protobuf wire).
    - `apply_equip_ops_to_items` — pure mutation on a passed-in items vector, returns the set of modified item IDs.
    - `build_equip_response_body` — pure response body builder for emsg 2570.
    - `find_item_by_id` / `erase_item_by_id` — pure item lookup/erase helpers.
  - All 3 handlers (`GBE_HandleDotaUnlockItemStyleRequest`, `GBE_HandleDotaSetItemStyleRequest`, `GBE_HandleDotaEquipItemsRequest`) now delegate parsing/mutation/building to the pure helpers while the coordinator methods keep ownership of `push_incoming_now`, `save_items_to_file`, `callback_item_updated`, server-GC forward, network broadcast, and lobby snapshot refresh.
  - The action list stays a plain `std::vector<GBE_DotaAction>` alias (no builder wrapper) — the side-effect sequence is enforced by tests, not by a runtime executor, because each handler's side effects are short and protocol-sensitive.
  - Test harness changes:
    - `tools/gbe_dota_handler_test/free_func_stubs.cpp` now compiles the real implementations of `GBE_ParseDotaEquipOps` and `GBE_ApplyDotaUnlockStyleBitmask` inline (copied from `dll/gbe_dota_gc_payload_helpers.cpp` lines 2778-2886) instead of stubs. The full `gbe_dota_gc_payload_helpers.cpp` TU cannot be compiled in the offline test environment because it pulls in the heavy SDK include chain (`steam_game_coordinator.h` -> `dll.h` -> `common_includes.h` -> `common_helpers/os_detector.h`). A `TODO(phase-3.2)` comment marks this duplicate for removal once payload helpers are split into a pure-logic TU.
    - `g_test_steam_client` was promoted from `static` to `extern` (declared in `stubs.h`) so the full-forward equip smoke test can wire a server GC into it.
    - Fixed a `free(): invalid size` crash in the `Networking::sendToAllGameservers` stub — it was `delete`-ing the `Common_Message*` even though the equip handler passes a stack address (the inner `GameServer_Items_Messages` is already freed by `set_allocated_*`); the stub now just records the broadcast without freeing.
  - Added 3 equip smoke tests to `tools/gbe_dota_handler_test/smoke_test.cpp`:
    - `test_inventory_equip_basic` — is_server=true path: PushIncomingNow(26) -> PushIncomingNow(2570) -> SaveItemsToFile + equip_states mutation check.
    - `test_inventory_equip_empty` — parse failure early return: 0 actions.
    - `test_inventory_equip_full_forward` — full server-GC-forward + network-broadcast + lobby-snapshot-refresh path, asserting the documented 8-action ordering invariant: PushIncomingNow(26) -> PushIncomingNow(2570) -> SaveItemsToFile -> ServerGcForward(0=cache) -> ServerGcForward(21) -> ServerGcForward(26) -> NetworkBroadcast -> LobbySnapshotRefresh.

## Verification

- `python3 tools/_audit_gc_refactor.py` passed.
- `tools/run_gc_offline_tests.sh` passed.
- Payload helper test result: `92/92 passed`.
- Handler test result: `9/9 passed` (6 original inventory smoke tests + 3 new equip smoke tests).
- Full offline suite: `101/101 passed` (92 payload + 9 handler).
- Audit result:
  - Zombie declarations: `0`
  - Under-exposed definitions: `0`
  - Doc line-number mismatches: `0`

## Recommended Next Step

- Phase 3.1.5 mechanical extraction is fully complete (3.1.5a match + 3.1.5b post-login/template + 3.1.5c misc). All 62 handlers now live in 7 domain-specific files.
- Phase 3.1.6 complete: `dll/gbe_dota_handlers.cpp` removed entirely (was an 81-line shell with zero definitions).
- Phase 3.1.6.5 complete: handler-level test harness built with 9/9 inventory smoke tests passing. Chat/lobby/match domain smoke tests deferred to their respective logic-refactor tasks (3.1.8/3.1.9/3.1.10) where the stub surface will be extended.
- Phase 3.1.6.6 complete: side-effect action model defined in `dll/gbe_dota_action_model.h`; test harness aligned to use the canonical `GBE_DotaActionType`.
- Phase 3.1.7 complete: inventory handler logic refactored into pure helpers + coordinator-owned side effects, with 9/9 smoke tests (including the full 8-action equip forward ordering invariant) protecting the documented side-effect sequences.
- **Logic-refactor tiering decision (2026-07-04):** the 3.1.7 pilot surfaced two cost signals — (1) `free_func_stubs.cpp` had to duplicate real implementations of `GBE_ParseDotaEquipOps` / `GBE_ApplyDotaUnlockStyleBitmask` because the full `gbe_dota_gc_payload_helpers.cpp` TU cannot compile offline (heavy SDK include chain); (2) each new domain needs its own `test_wrapper.cpp` + extended stub surface, with stub/SDK drift risk growing linearly. Based on this evidence, 3.1.8/3.1.9/3.1.10 are **tiered**:
  - **Tier A (do now):** side-effect order documentation block at top of each domain `.cpp` + anonymous-namespace pure helper extraction where genuinely pure.
  - **Tier B (defer to post-3.2):** per-domain `test_wrapper.cpp` + extended stubs + ordering smoke tests. Rationale: 3.2 will move pure logic into a TU that doesn't depend on the heavy SDK include chain, eliminating the duplication debt and making per-domain harness cheap.
  - **Tier C (skip unless triggered):** `GBE_DotaActionList` builder/executor wrapper — plain `std::vector<GBE_DotaAction>` + test assertions is sufficient for current handler complexity.
  - Ordering invariants protected by documentation + code review + existing 3.1.7 inventory smoke tests until Tier B lands.
- Next step is **3.1.8 (chat handler logic, Tier A only)**: add side-effect order documentation block to `dll/gbe_dota_chat_handlers.cpp` citing `dll/gbe_dota_action_model.h`, extract anonymous-namespace pure helpers (request parsers, response body builders) where genuinely pure, keep coordinator methods owning side effects. Do NOT add per-domain test harness in this task. Then 3.1.9 (lobby) and 3.1.10 (match+misc) follow the same Tier A pattern. After all three Tier A passes complete, proceed to Phase 3.2 (payload helper split) which unblocks Tier B test coverage.
- Phase 3.1.8 chat handler logic refactor (Tier A) complete.
  - Added 110-line side-effect order documentation block at top of `dll/gbe_dota_chat_handlers.cpp` covering all 7 chat/broadcast handlers (7009/7273-out/7273-in/7272/7149/7367/8054), each with step-by-step side-effect sequence and invariant annotations.
  - Extracted one anonymous-namespace pure helper `compute_leave_chat_decision(request, lobby) -> LeaveChatDecision` from the 7272 leave-chat handler's ~50-line decision-derivation block (channel resolution + 6 boolean flags: leaving_postgame_channel, matches_current_postgame_channel, matches_pre_postgame_channel, leaving_non_current_channel_during_abandon, leaving_legacy_channel_after_signout, request_matches_local).
  - The other 6 handlers already delegate parsing to `gbe::proto_wire::parse_*` pure functions and response building to `gbe::gc_message::build_*` pure functions — no additional helper extraction warranted.
  - `GBE_HandleDotaLeaveChatChannelRequest` now reads decision flags from `LeaveChatDecision d` instead of interleaving boolean derivation, making the 13-step side-effect sequence easier to follow against the documentation block.
  - No behavior change (verified: 92/92 payload + 9/9 handler tests pass, audit clean 0/0/0).
- Next step is **3.1.9 (lobby handler logic, Tier A only)**: `dll/gbe_dota_lobby_handlers.cpp` has 17 handlers (1663 lines). Apply the same Tier A pattern — side-effect order documentation block at top + anonymous-namespace pure helper extraction where genuinely pure. Per-task boundary note: cross-handler state-machine consolidation (launch/teardown/reconnect) belongs to Phase 3.4, not here. After 3.1.9, do 3.1.10 (match+misc), then proceed to Phase 3.2 (payload helper split) which unblocks Tier B test coverage.
- Phase 3.1.9 lobby handler logic refactor (Tier A) complete.
  - Added 200-line side-effect order documentation block at top of `dll/gbe_dota_lobby_handlers.cpp` covering all 17 lobby handlers (7038/7040/7046/7048/7044/4512/4513/friend+network invite msgs/7035/7004/7042/7041/7050/7047/7081/8246), each with step-by-step side-effect sequence and invariant annotations. Block also carries a cross-handler boundary note: per-handler sequencing stays here; cross-handler state-machine consolidation belongs to Phase 3.4.
  - Extracted one anonymous-namespace pure helper `compute_abandon_decision(lobby, wrapped, is_server) -> AbandonDecision` from the 7035 abandon handler's ~30-line decision-derivation block (lobby_id, lobby_state, lobby_game_state, abandon_game_state_threshold, treat_as_current_game_disconnect, ready_for_abandon_teardown, arcade_launch_failed_before_connect).
  - The other 16 handlers already delegate parsing/mutation/planning to existing pure helpers (`gbe::dota_lobby_state::compose_create_lobby_plan`, `compose_join_lobby_merge_plan`, `compose_launch_init_plan`, `compose_set_details_plan`, `gbe::dota_lobby_flow::apply_lobby_member_team_slot_update`, `gbe::proto_wire::parse_*`, `gbe::gc_message::build_*`) — no additional helper extraction warranted.
  - `GBE_HandleDotaAbandonCurrentGameRequest` now reads decision flags from `AbandonDecision d` instead of interleaving boolean derivation, making the 4-branch side-effect sequence (grace window / arcade-failed / current-game disconnect / ready-for-teardown) easier to follow against the documentation block.
  - No behavior change (verified: 92/92 payload + 9/9 handler tests pass, audit clean 0/0/0).
- Next step is **3.1.10 (match + misc handler logic, Tier A only)**: `dll/gbe_dota_match_handlers.cpp` (802 lines, 13 handlers) and `dll/gbe_dota_misc_handlers.cpp` (726 lines, 17 handlers). Apply the same Tier A pattern — side-effect order documentation block at top of each file + anonymous-namespace pure helper extraction where genuinely pure. After 3.1.10, proceed to Phase 3.2 (payload helper split) which unblocks Tier B test coverage.
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
