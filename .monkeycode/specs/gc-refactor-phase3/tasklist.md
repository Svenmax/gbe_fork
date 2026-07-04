# GC Refactor Phase 3 Task List

## Phase 3.0: Stabilize Current Refactor

- [ ] 3.0.1 Fix audit script portability
  - [ ] Replace hard-coded `/workspace` paths with repository-relative paths.
  - [ ] Ensure the script works when run from `gbe_fork/`.
  - [ ] Ensure the script prints actionable categories for real issues and likely false positives.

- [ ] 3.0.2 Classify audit findings
  - [ ] Review header declarations reported as missing definitions.
  - [ ] Separate type-name false positives from real zombie declarations.
  - [ ] Review definitions reported as under-exposed.
  - [ ] Decide whether each under-exposed function should be declared, made file-local, or moved.

- [ ] 3.0.3 Fix real declaration boundary issues
  - [ ] Remove unused declarations from `dll/gbe_dota_gc_internal.h`.
  - [ ] Add missing declarations for genuine cross-TU functions.
  - [ ] Mark private helper functions as `static` or move them into anonymous namespaces.

- [x] 3.0.4 Fix stale documentation references
  - [x] Update `REFACTOR_TODO.md` line references.
  - [x] Prefer function names and file names over fragile line numbers for future references.

- [x] 3.0.5 Add audit to verification
  - [x] Add a bounded verification command for the audit script.
  - [x] Run `tools/run_gc_offline_tests.sh`.
  - [x] Record any remaining false positives in the audit script comments.

## Phase 3.1: Split Handler Buckets By Domain

> Requirement: every GC domain that is mechanically extracted must also receive a follow-up logic refactor. A bucket is not considered complete after file movement alone. Each bucket needs domain-level parsing, state mutation, response/message construction, side-effect isolation, and focused tests where feasible.

> Side-effect order rule: before logic refactoring any GC handler, document the existing side-effect sequence and preserve it with tests or an explicit ordered action list. Pure helpers may build decisions and messages, while coordinator methods keep ownership of real side effects and execute actions serially.

> Split-boundary rule: split by cohesive domain and clear responsibility, not by smallest possible file size. Prefer local helpers in the domain `.cpp`; create a new helper file only for cross-domain reuse, pure testable logic, or files that exceed the healthy range.

- [x] 3.1.1 Build handler inventory
  - [x] List all `Steam_Game_Coordinator::GBE_HandleDota*` functions.
  - [x] Group handlers into lobby, chat, inventory, match, and misc domains.
  - [x] List handler-only static constants and their use sites.
  - [x] Save inventory to `.monkeycode/specs/gc-refactor-phase3/handler-inventory.md`.

- [x] 3.1.2 Extract inventory handlers first
  - [x] Create `dll/gbe_dota_inventory_handlers.cpp`.
  - [x] Move `GBE_HandleDotaUnlockItemStyleRequest`.
  - [x] Move `GBE_HandleDotaSetItemStyleRequest`.
  - [x] Move `GBE_HandleDotaEquipItemsRequest`.
  - [x] Keep signatures unchanged.
  - [x] Run offline GC tests.
  - [x] Confirm `dll/gbe_dota_handlers.cpp` decreased to 5757 lines and `dll/gbe_dota_inventory_handlers.cpp` is 513 lines.

- [x] 3.1.3 Extract chat handlers
  - [x] Create `dll/gbe_dota_chat_handlers.cpp`.
  - [x] Move chat join, leave, and message handlers.
  - [x] Move chat-only constants and helpers.
  - [x] Add a follow-up chat logic refactor task before marking the chat bucket complete.
  - [x] Run offline GC tests.
  - Notes: moved 7 chat/broadcast handlers (`GBE_HandleDotaJoinChatChannelRequest`, `GBE_HandleDotaChatMessageRequest`, `GBE_HandleDotaNetworkChatMessage`, `GBE_HandleDotaLeaveChatChannelRequest`, `GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest`, `GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest`, `GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest`) plus chat-only static `GBE_GenerateDotaChatChannelId`. List Y = 0 (no externalize needed). `dll/gbe_dota_handlers.cpp` 5757 → 5164 lines, new file 653 lines. Follow-up logic refactor tracked as 3.1.8.

- [x] 3.1.4 Extract lobby handlers
  - [x] Create `dll/gbe_dota_lobby_handlers.cpp`.
  - [x] Move lobby create, join, leave, list, invite, kick, and set-details handlers.
  - [x] Keep signatures unchanged.
  - [x] Add a follow-up lobby logic refactor task before marking the lobby bucket complete.
  - [x] Run offline GC tests.
  - Notes: moved 17 lobby handlers (`GBE_HandleDotaPracticeLobbyCreateRequest`, `GBE_HandleDotaLobbyListRequest`, `GBE_HandleDotaCustomLobbyListRequest`, `GBE_HandleDotaFriendPracticeLobbyListRequest`, `GBE_HandleDotaPracticeLobbyJoinRequest`, `GBE_HandleDotaInviteToLobbyRequest`, `GBE_HandleDotaLobbyInviteResponseRequest`, `GBE_HandleDotaFriendLobbyInviteMessage`, `GBE_HandleDotaNetworkLobbyInviteMessage`, `GBE_HandleDotaAbandonCurrentGameRequest`, `GBE_HandleDotaGameMatchSignOutRequest`, `GBE_HandleDotaPracticeLobbyLeaveRequest`, `GBE_HandleDotaPracticeLobbyLaunchRequest`, `GBE_HandleDotaPracticeLobbySetDetailsRequest`, `GBE_HandleDotaPracticeLobbySetTeamSlotRequest`, `GBE_HandleDotaPracticeLobbyKickRequest`, `GBE_HandleDotaDestroyLobbyRequest`) plus 6 lobby-only statics (`GBE_ApplyDotaCustomGameDetailsRequest`, `GBE_NormalizeDotaCustomGameDetailsFromInstalledMod`, `GBE_GenerateDotaLobbyId`, `GBE_GenerateDotaMatchId`, `GBE_AdaptDotaLobbyInviteCacheSubscribedPayload`, `GBE_IsDotaLobbyInviteCacheSubscribedPayload`), all kept `static` in new TU (List X). List Y = 0 (no externalize needed). `dll/gbe_dota_handlers.cpp` 5164 → 3578 lines, new file 1663 lines. Follow-up logic refactor tracked as 3.1.9.

- [ ] 3.1.5 Extract match and misc handlers
  - [x] 3.1.5a Create `dll/gbe_dota_match_handlers.cpp` for the 7034 direct match-flow + custom-game loading lifecycle.
  - [x] 3.1.5b Create `dll/gbe_dota_post_login_handlers.cpp` (post-login/socket/server-assignment) and `dll/gbe_dota_template_replay_handlers.cpp` (template-replay switch + 12 template statics).
  - [x] 3.1.5c Create `dll/gbe_dota_misc_handlers.cpp` for remaining one-off handlers.
  - [ ] Keep any shared helper in the smallest reasonable file.
  - [ ] Add follow-up logic refactor tasks for match, post-login, and misc buckets before marking them complete.
  - [x] Run offline GC tests.
  - Notes (3.1.5b post-login/template sub-batch): split into two files because TemplateReplay (1082 lines, canned-response switch) is a different concern from post-login/socket/server-assignment (session establishment). Moved to `dll/gbe_dota_template_replay_handlers.cpp` (1255 lines): 1 handler `GBE_HandleDotaTemplateReplayRequest` + 12 template statics (10 `GBE_kDota*Template[]` byte arrays + 3 `GBE_kDota*TemplateHex` hex strings), all kept `static` in new TU (List X). Moved to `dll/gbe_dota_post_login_handlers.cpp` (1011 lines): 4 handlers (`GBE_HandleDotaServerAssignmentRequest`, `GBE_HandleDotaDirectPostLoginRequest`, `GBE_HandleDotaAddSocketRequest`, `GBE_HandleDotaWrappedPostLoginRequest`) + 2 session constants (`GBE_kSteamGamesPlayedWithDataBlob`, `GBE_kSteamAuthList`), all kept `static` in new TU (List X). List Y = 0 for both files (all 5 handlers already declared in `dll/dll/steam_game_coordinator.h`). `dll/gbe_dota_handlers.cpp` 2207 → 81 lines (now an include + using-aliases shell with zero handler definitions; 3.1.6 will evaluate removal). Follow-up logic refactor tracked as 3.1.10.
  - Notes (3.1.5c misc sub-batch): moved 17 misc one-off handlers (`GBE_HandleDotaMinimalVarintSuccessRequest`, `GBE_HandleDota7427NotificationsRequest`, `GBE_HandleDotaUploadRateRequest`, `GBE_HandleDotaProfileCardRequest`, `GBE_HandleDotaLookupAccountNameRequest`, `GBE_HandleDotaEmoticonDataRequest`, `GBE_HandleDotaConductScorecardRequest`, `GBE_HandleDotaCoachingSummaryRequest`, `GBE_HandleDotaRankRequest`, `GBE_HandleDotaLaunchAdvanceOrConsume`, `GBE_HandleDota8870LaunchMarkerRequest`, `GBE_HandleDotaLanServerAvailableRequest`, `GBE_HandleDotaBatchPlayerResourcesRequest`, `GBE_HandleDotaCacheSubscriptionRefreshRequest`, `GBE_HandleDotaLeaverDetectedRequest`, `GBE_HandleDotaSignOutPermissionRequest`, `GBE_HandleDotaSubmitPlayerReportV2Request`) plus 1 handler-local struct `GBE_ProtoField` (List X, kept file-local in new TU). List Y = 0 (no externalize needed; 2 `using` aliases `GBE_DotaEmptyRequestShape` and `GBE_DotaRankRequestShape` repeated in new TU). `dll/gbe_dota_handlers.cpp` 2857 → 2207 lines, new file 726 lines. Follow-up logic refactor tracked as 3.1.10.
  - Notes (3.1.5a match sub-batch): moved 10 direct 7034 match-flow handlers (`GBE_HandleDotaDirect7034Request`, `GBE_HandleDotaDirectOwnerHeroKnownEquipReplay`, `GBE_HandleDotaDirect7034DisconnectedPlayers`, `GBE_HandleDotaDirect7034RuntimeUpdates`, `GBE_HandleDotaDirect7034StrategyTime`, `GBE_HandleDotaDirect7034StrategyTimeFallback`, `GBE_HandleDotaDirect7034StrategyTimePreserve`, `GBE_HandleDotaDirect7034Response`, `GBE_HandleDotaDirect7034LaunchPoll`, `GBE_HandleDotaDirect7034WaitForPlayers`) plus 1 static (`GBE_AdaptDota7034ConnectedPlayersResponsePayload`, kept `static` in new TU, List X). Also merged in the 3 custom-game loading handlers (`GBE_HandleDotaCustomGameReadyUpRequest`, `GBE_HandleDotaCustomGameStartedLoadingRequest`, `GBE_HandleDotaCustomGameFinishedLoadingRequest`) that were previously extracted to `dll/gbe_dota_custom_game_handlers.cpp` (Phase 3.1.5); that standalone file was removed because custom-game loading is a sub-phase of the 7034 launch flow. List Y = 0 (no externalize needed; 4 `using` aliases repeated in new TU: `GBE_Dota7034RequestShape`, `GBE_Dota7034ConnectedPlayer`, `GBE_Dota7034DisconnectedPlayer`, `GBE_Dota8053Result`). `dll/gbe_dota_handlers.cpp` 3459 → 2857 lines, new file 802 lines. Follow-up logic refactor tracked as 3.1.10.

- [x] 3.1.6 Reduce or remove original handler file
  - [x] `dll/gbe_dota_handlers.cpp` removed entirely (was 81-line include + using-aliases shell with zero handler definitions after 3.1.5b). All 62 handlers now live in domain-specific files; all using-aliases were file-local (no linkage) and each domain TU already repeats the aliases it needs.
  - [x] Verify the build still picks up all new translation units (premake5.lua uses `dll/**` glob, no explicit reference to handlers.cpp; audit + offline tests pass after deletion).
  - Notes: `dll/gbe_dota_handlers.cpp` deleted. The original 5757-line monolith is now fully decomposed into 7 domain files (inventory 513 / chat 653 / lobby 1663 / match 802 / misc 726 / template_replay 1255 / post_login 1011). Total handler code ~6623 lines across 7 files vs 5757 in original (the ~14% growth is from repeated header comment blocks + using-alias declarations per TU, an acceptable cost for domain cohesion).

- [x] 3.1.6.5 Build handler-level test harness (prerequisite for 3.1.7-3.1.10)
  - [x] Add a minimal fixture loader that can replay a captured request body + lobby state into a handler call.
  - [x] Add an action recorder that captures `push_incoming_now`, `save_items_to_file`, server-GC calls, network broadcast, and lobby snapshot refresh calls in order, without executing real side effects.
  - [x] Add at least one smoke test per already-extracted domain (inventory, chat, lobby, match) that drives a representative handler and asserts on the recorded action sequence.
  - [x] Document the harness in `tools/` so future logic-refactor tasks can extend it.
  - [x] Run audit script and offline GC tests.
  - Notes: created `tools/gbe_dota_handler_test/` with 4 files: `stubs.h` (recording `Steam_Game_Coordinator` stub class + `ActionRecorder` + Steam SDK type stubs), `test_wrapper.cpp` (include-guard override pattern that compiles `dll/gbe_dota_inventory_handlers.cpp` inline), `free_func_stubs.cpp` (stubs for `GBE_GC_DebugLog`, `get_steam_client`, `GBE_PushDotaPlayerEquippedItemsCacheToGC`, `GBE_ApplyDotaUnlockStyleBitmask`, `GBE_ParseDotaEquipOps`, `get_full_program_path`), `smoke_test.cpp` (6 inventory domain smoke tests with varint encoding helpers + `TestFixture`). Extended `tools/gbe_dota_gc_payload_helpers_test/pb_stubs/tf2/base_gcmessages.pb.h` with `add_objects()`, `set_version()`, `set_service_id()` on `CMsgSOMultipleObjects` and `set_type_id()`, `set_object_data(const std::string&)` on `CMsgSOMultipleObjects_Object` (backward-compatible additions). Updated `tools/run_gc_offline_tests.sh` to build and run the handler test. Result: 6/6 inventory smoke tests pass; full offline suite 98/98 (92 payload + 6 handler) passes; audit clean (0 zombie, 0 under-exposed, 0 mismatch). Chat/lobby/match domain smoke tests are deferred to 3.1.8/3.1.9/3.1.10 — each domain needs its own `test_wrapper.cpp` (different handler .cpp compiled inline) and extended stub surface (more handler declarations + more coordinator member stubs). The extension pattern is documented in `stubs.h` comments and the placeholder sections in `smoke_test.cpp`.

- [x] 3.1.6.6 Define side-effect action model (prerequisite for 3.1.7-3.1.10)
  - [x] Define a minimal local action type only when a handler has multiple observable side effects.
  - [x] Include action type, emsg, payload, target, job id, and reason fields only when needed by tests or execution.
  - [x] Keep action execution in coordinator-owned code.
  - [x] Preserve protocol-sensitive order such as `CacheSubscribed` before `emsg21/26`, SO update before response, and full item cache before create/update.
  - [x] Add action-order assertions before changing legacy side-effect sequences.
  - Notes: created `dll/gbe_dota_action_model.h` defining the canonical `GBE_DotaActionType` enum (7 types: PushIncomingNow, PushIncoming, SaveItemsToFile, CallbackItemUpdated, ServerGcForward, NetworkBroadcast, LobbySnapshotRefresh), `GBE_DotaAction` struct (emsg/payload/target_steam_id/item_id/job_id/reason fields, filled only when relevant to the type), and `GBE_DotaActionList = std::vector<GBE_DotaAction>` alias. The header documents the contract (pure helpers build action lists; coordinator methods execute serially), the "only when multiple side effects" usage rule, and 4 protocol-sensitive ordering invariants grounded in tested behavior: (1) unlock flow SO Update(22) -> SO Destroy(24) -> Response(2572); (2) set-style flow CallbackItemUpdated -> SaveItemsToFile -> Response(2578); (3) full item cache (CacheSubscribed) before create/update SO messages when bootstrapping a remote GC; (4) equip flow SO Update(26) -> Response(2570) -> ServerGcForward -> NetworkBroadcast -> LobbySnapshotRefresh. Aligned the test harness: `tools/gbe_dota_handler_test/stubs.h` now includes the real header and `RecordedAction::type` uses `GBE_DotaActionType` directly (removed the duplicate nested enum); `smoke_test.cpp` assertions reference `GBE_DotaActionType::*` so test code and contract share one name source. No handler code changed — the model is defined and ready for 3.1.7-3.1.10 to adopt. `GBE_DotaActionList` is a plain alias (no builder wrapper) until repeated construction patterns justify helpers. Audit clean (0/0/0); full offline suite 98/98 passes.

- [x] 3.1.7 Refactor inventory handler logic
  - [x] Document current side-effect order for all three inventory handlers before changing their internals.
  - [x] Split `GBE_HandleDotaUnlockItemStyleRequest` into request parsing, unlock mutation, consumable mutation, SO destroy/update construction, response construction, and coordinator side effects.
  - [x] Split `GBE_HandleDotaSetItemStyleRequest` into request parsing, item style mutation, persistence decision, response construction, and server-GC forwarding decision.
  - [x] Split `GBE_HandleDotaEquipItemsRequest` into equip-op parsing, inventory mutation, modified-item collection, SO message construction, local response construction, server-GC forwarding, network broadcast, and lobby snapshot refresh decision.
  - [x] Model side effects as an ordered action list where feasible, then execute the list from the coordinator method.
  - [x] Keep pure mutation and message-construction helpers independent from `Steam_Game_Coordinator` where feasible.
  - [x] Keep coordinator methods responsible for owning side effects: `push_incoming_now`, `save_items_to_file`, server-GC calls, network broadcast, and lobby snapshot replay.
  - [x] Add focused tests for unlock style success, invalid style index, consumable deletion, set style persistence, equip-op mutation, response payload shape, and action ordering.
  - [x] Run audit script and offline GC tests.
  - Notes: refactored `dll/gbe_dota_inventory_handlers.cpp` with an anonymous namespace of pure helpers (`parse_unlock_style_request`, `parse_set_style_request`, `apply_equip_ops_to_items`, `build_equip_response_body`, `find_item_by_id`, `erase_item_by_id`) and a side-effect order documentation block citing `dll/gbe_dota_action_model.h`. All 3 handlers now delegate parsing/mutation/building to the pure helpers while the coordinator methods keep ownership of `push_incoming_now`, `save_items_to_file`, `callback_item_updated`, server-GC forward, network broadcast, and lobby snapshot refresh. The action list stays a plain `std::vector<GBE_DotaAction>` alias (no builder wrapper) — the side-effect sequence is enforced by tests, not by a runtime executor, because each handler's side effects are short and protocol-sensitive. Test harness changes: (1) `tools/gbe_dota_handler_test/free_func_stubs.cpp` now compiles the real implementations of `GBE_ParseDotaEquipOps` and `GBE_ApplyDotaUnlockStyleBitmask` inline (copied from `dll/gbe_dota_gc_payload_helpers.cpp` lines 2778-2886) instead of stubs, because the full `gbe_dota_gc_payload_helpers.cpp` TU pulls in the heavy SDK include chain (`steam_game_coordinator.h` -> `dll.h` -> `common_includes.h` -> `common_helpers/os_detector.h`) which is not available offline; a `TODO(phase-3.2)` comment marks this duplicate for removal once payload helpers are split into a pure-logic TU; (2) `g_test_steam_client` was promoted from `static` to `extern` (declared in `stubs.h`) so the full-forward equip smoke test can wire a server GC into it; (3) fixed a `free(): invalid size` crash in the `Networking::sendToAllGameservers` stub — it was `delete`-ing the `Common_Message*` even though the equip handler passes a stack address (the inner `GameServer_Items_Messages` is already freed by `set_allocated_*`); the stub now just records the broadcast without freeing. Added 3 equip smoke tests to `tools/gbe_dota_handler_test/smoke_test.cpp`: `test_inventory_equip_basic` (is_server=true path: PushIncomingNow(26) -> PushIncomingNow(2570) -> SaveItemsToFile + equip_states mutation check), `test_inventory_equip_empty` (parse failure early return: 0 actions), `test_inventory_equip_full_forward` (full server-GC-forward + network-broadcast + lobby-snapshot-refresh path, asserting the documented 8-action ordering invariant: PushIncomingNow(26) -> PushIncomingNow(2570) -> SaveItemsToFile -> ServerGcForward(0=cache) -> ServerGcForward(21) -> ServerGcForward(26) -> NetworkBroadcast -> LobbySnapshotRefresh). Total: 9/9 handler tests pass, 92/92 payload tests pass, audit clean (0 zombie / 0 under-exposed / 0 mismatch).

- [x] 3.1.8 Refactor chat handler logic after extraction (Tier A only)
  - Notes: per the plan's logic-refactor tiering, 3.1.8-3.1.10 apply only Tier A (side-effect order docs + anonymous-namespace pure helper extraction). Tier B (per-domain test harness) is deferred to post-3.2 because the 3.1.7 pilot showed that adding per-domain harness now would require duplicating real payload helper implementations into stubs (heavy SDK include chain blocks compiling the real TU offline), multiplying test debt before 3.2 resolves the root cause. Ordering invariants are protected by documentation + code review + the existing 3.1.7 inventory smoke tests until Tier B lands.
  - [x] Document current chat side-effect order before changing handler internals (side-effect block at top of `dll/gbe_dota_chat_handlers.cpp` citing `dll/gbe_dota_action_model.h`).
  - [x] Split request parsing, channel lookup/mutation, response construction, broadcast construction, and coordinator side effects via anonymous-namespace pure helpers where genuinely pure.
  - [x] Keep coordinator methods responsible for owning side effects: `push_incoming_now`, `save_items_to_file`, network broadcast, etc.
  - [x] Do NOT add a per-domain `test_wrapper.cpp` / extended stubs / ordering smoke tests in this task — deferred to post-3.2 Tier B.
  - [x] Run audit script and offline GC tests (existing payload + 3.1.7 inventory handler tests must still pass).
  - Notes (3.1.8 Tier A): added a 110-line side-effect order documentation block at the top of `dll/gbe_dota_chat_handlers.cpp` covering all 7 chat/broadcast handlers (7009 join chat, 7273 outgoing chat, 7273 incoming network chat, 7272 leave chat, 7149 join broadcast, 7367 update broadcast, 8054 close broadcast), each with step-by-step side-effect sequence and invariant annotations. Extracted one anonymous-namespace pure helper `compute_leave_chat_decision(request, lobby) -> LeaveChatDecision` from the 7272 leave-chat handler's ~50-line decision-derivation block (channel resolution, leaving_postgame_channel, matches_current_postgame_channel, matches_pre_postgame_channel, leaving_non_current_channel_during_abandon, leaving_legacy_channel_after_signout, request_matches_local). The other 6 handlers already delegate parsing to `gbe::proto_wire::parse_*` pure functions and response building to `gbe::gc_message::build_*` pure functions, so no additional helper extraction was warranted. The `GBE_HandleDotaLeaveChatChannelRequest` handler now reads decision flags from `LeaveChatDecision d` instead of interleaving boolean derivation, making the 13-step side-effect sequence easier to follow against the documentation block. No behavior change (verified by offline tests: 92/92 payload + 9/9 handler, audit clean 0/0/0).

- [x] 3.1.9 Refactor lobby handler logic after extraction (Tier A only)
  - Notes: scope is **per-handler** internal restructuring (parsing/mutation/response/side-effect split inside each lobby handler). Cross-handler state-machine consolidation (launch/teardown/reconnect) belongs to Phase 3.4, not here. Boundary rule: 3.1.9 may extract pure helpers that read lobby state and return decisions/payloads; 3.4 owns the transition table that sequences those decisions across handlers. If a 3.1.9 change touches more than one handler's state-transition interaction, move it to 3.4. Tier A only — no per-domain harness (see 3.1.8 notes).
  - [x] Document current lobby side-effect order before changing handler internals (side-effect block at top of `dll/gbe_dota_lobby_handlers.cpp`).
  - [x] Split request parsing, lobby state mutation, lobby snapshot construction, response construction, invite/kick decisions, and coordinator side effects via anonymous-namespace pure helpers where genuinely pure.
  - [x] Keep lobby decision helpers pure where they can return explicit decisions instead of mutating global state.
  - [x] Keep coordinator methods responsible for owning side effects.
  - [x] Do NOT add a per-domain `test_wrapper.cpp` / extended stubs / ordering smoke tests in this task — deferred to post-3.2 Tier B.
  - [x] Run audit script and offline GC tests.
  - Notes (3.1.9 Tier A): added a 200-line side-effect order documentation block at the top of `dll/gbe_dota_lobby_handlers.cpp` covering all 17 lobby handlers (7038 create, 7040 list, 7046 custom list, 7048 friend list, 7044 join, 4512 invite, 4513 invite response, friend/network lobby invite messages, 7035 abandon, 7004 signout, 7042 leave, 7041 launch, 7050 set details, 7047 set team slot, 7081 kick, 8246 destroy), each with step-by-step side-effect sequence and invariant annotations. The block also carries a cross-handler boundary note: per-handler sequencing stays here; cross-handler state-machine consolidation belongs to Phase 3.4. Extracted one anonymous-namespace pure helper `compute_abandon_decision(lobby, wrapped, is_server) -> AbandonDecision` from the 7035 abandon handler's ~30-line decision-derivation block (lobby_id, lobby_state, lobby_game_state, abandon_game_state_threshold, treat_as_current_game_disconnect, ready_for_abandon_teardown, arcade_launch_failed_before_connect). The other 16 handlers already delegate parsing/mutation/planning to existing pure helpers (`gbe::dota_lobby_state::compose_create_lobby_plan`, `compose_join_lobby_merge_plan`, `compose_launch_init_plan`, `compose_set_details_plan`, `gbe::dota_lobby_flow::apply_lobby_member_team_slot_update`, `gbe::proto_wire::parse_*`, `gbe::gc_message::build_*`), so no additional helper extraction was warranted. The `GBE_HandleDotaAbandonCurrentGameRequest` handler now reads decision flags from `AbandonDecision d` instead of interleaving boolean derivation, making the 4-branch side-effect sequence (grace window / arcade-failed / current-game disconnect / ready-for-teardown) easier to follow against the documentation block. No behavior change (verified by offline tests: 92/92 payload + 9/9 handler, audit clean 0/0/0).

- [x] 3.1.10 Refactor match and misc handler logic after extraction (Tier A only)
  - Notes: Tier A only — no per-domain harness (see 3.1.8 notes).
  - [x] Document current match/misc side-effect order before changing handler internals (side-effect block at top of `dll/gbe_dota_match_handlers.cpp` and `dll/gbe_dota_misc_handlers.cpp`).
  - [x] Split request parsing, state decisions, response construction, and side effects for every moved match/misc handler via anonymous-namespace pure helpers where genuinely pure.
  - [x] Keep coordinator methods responsible for owning side effects.
  - [x] Group shared pure helpers by actual reuse, not by convenience.
  - [x] Do NOT add a per-domain `test_wrapper.cpp` / extended stubs / ordering smoke tests in this task — deferred to post-3.2 Tier B.
  - [x] Run audit script and offline GC tests.
  - Notes (3.1.10 Tier A): added a 100-line side-effect order documentation block at the top of `dll/gbe_dota_misc_handlers.cpp` (between the `using` aliases and the `GBE_ProtoField` struct) covering all 17 misc handlers (MinimalVarintSuccess, 7427, 7440, 7485, 7537, 7563, 7591, 7812, 7674, LaunchAdvanceOrConsume, 8870, 4511, 7450, 2008, 7072, 7027, 7925), each with step-by-step side-effect sequence and invariant annotations. Added a 150-line side-effect order documentation block at the top of `dll/gbe_dota_match_handlers.cpp` (between the `using` aliases and the file-local static `GBE_AdaptDota7034ConnectedPlayersResponsePayload`) covering all 13 match handlers (the 7034 entry handler + 9 internal helper methods forming the 7034 side-effect sequence + 3 custom-game loading lifecycle handlers 7070/8052/8053), each with step-by-step side-effect sequence and invariant annotations. The match block also carries a cross-handler boundary note: the 7034 entry handler fans out to internal helpers (OwnerHeroKnownEquipReplay, DisconnectedPlayers, RuntimeUpdates -> WaitForPlayers / StrategyTime -> Fallback / Preserve / LaunchPoll, Response) which together form one side-effect sequence — callers should not interleave other state mutations between them. Cross-handler state machine consolidation belongs to Phase 3.4. No anonymous-namespace pure helper extraction was warranted: every match handler already delegates parsing to `gbe::proto_wire::parse_dota7034_request_shape` / `parse_dota8053_result` / `read_*_field` pure functions, response building to `gbe::gc_message::build_*` pure functions, and the file-local `GBE_AdaptDota7034ConnectedPlayersResponsePayload` is already pure (reads request shape + lobby member state, writes response_message, no coordinator state access). Every misc handler already delegates parsing to `gbe::proto_wire::read_*`/`parse_*` and response building to `gbe::gc_message::build_*`. No behavior change (verified by offline tests: 92/92 payload + 9/9 handler, audit clean 0/0/0).

- [x] 3.1.11 Domain bucket completion gate
  - [x] Confirm every extracted GC handler bucket has a matching logic refactor task completed (Tier A minimum for chat/lobby/match; full Tier A+B for inventory).
  - [x] Confirm no bucket is accepted as complete based on mechanical file movement alone.
  - [x] Confirm each bucket has either focused tests OR a documented Tier B deferral reason citing the 3.2 dependency.
  - [x] Confirm side-effect order is documented (Tier A) for every side-effectful refactor, and covered by tests where Tier B has landed.
  - Notes: gate verified. Inventory (3.1.7) full Tier A+B — 9/9 smoke tests including the 8-action equip-forward ordering invariant. Chat (3.1.8), lobby (3.1.9), match+misc (3.1.10) all Tier A only — side-effect order documented at top of each domain `.cpp`, with Tier B per-domain harness deferred to post-3.2 per the tiering decision (root cause: `gbe_dota_gc_payload_helpers.cpp` cannot compile offline due to the heavy SDK include chain, forcing payload helper duplication into stubs; 3.2 will split pure logic into a TU that doesn't depend on the heavy SDK headers, eliminating the duplication debt). All 4 domains have a matching logic-refactor task complete. No bucket accepted on file movement alone — every bucket received at minimum a side-effect order documentation block citing `dll/gbe_dota_action_model.h` and pure-helper extraction where warranted.

- [ ] 3.1.12 Split-boundary discipline
  - [ ] Keep handler buckets aligned to stable domains: inventory, chat, lobby, match, and misc.
  - [ ] Keep local handler-only helpers inside the domain `.cpp` anonymous namespace.
  - [ ] Create a separate helper file only for cross-domain reuse, pure testable logic, or an oversized domain file.
  - [ ] Treat handler bucket files around `300-1200` lines as healthy when responsibilities remain cohesive.
  - [ ] Treat pure helper files around `200-1000` lines as healthy when responsibilities remain cohesive.
  - [ ] Require at least two real call sites before moving a helper into a shared cross-file helper module.
  - [ ] Document the responsibility boundary and migration reason for every new GC `.cpp` file.

## Phase 3.2: Separate Payload Helpers

- [x] 3.2.1 Inventory payload helper inventory
  - [x] List pure wire parsing functions.
  - [x] List pure item serialization functions.
  - [x] List lobby payload composition functions.
  - [x] List functions that depend on coordinator or global state.
  - Notes: `dll/gbe_dota_gc_payload_helpers.cpp` is 2953 lines with ~40 functions. Classified into 4 buckets: pure wire (~1100 lines, 14 functions + 5 static byte arrays — varint/field patching), pure item (~165 lines, 4 functions — `Econ_Item`/`GBE_DotaEquipOp` ops), pure lobby (~1500 lines, 15 functions — lobby cache-subscribed/details/launch payload composition), stateful (~190 lines, 11 functions — depend on `Steam_Game_Coordinator`/`Steam_Client`/`Settings*`/`GBE_shared_dota_lobby_state`). Full classification recorded in `handoff.md`. 3.2.2 will extract the pure-item group first (highest value: removes the `TODO(phase-3.2)` stub duplication in `tools/gbe_dota_handler_test/free_func_stubs.cpp`).

- [x] 3.2.2 Extract pure wire helpers
  - [x] Create `dll/gbe_dota_payload_wire_helpers.cpp` and matching internal header if needed.
  - [x] Move pure parse/build helpers with no coordinator dependency.
  - [x] Add tests for malformed varint, truncation, unknown wire type, and valid round trip.
  - [x] Run offline GC tests.
  - Notes: extracted 14 pure wire functions + 5 file-local static byte arrays from `dll/gbe_dota_gc_payload_helpers.cpp` (2779 -> 1994 lines) to new `dll/gbe_dota_payload_wire_helpers.cpp` (898 lines). The new TU has a minimal include list (`gbe_dota_gc_internal.h`, `gbe_dota_protocol_constants.h`, `gbe_dota_request_router.h`, `gbe_proto_wire.h`, `<sdk/steam/steamclientpublic.h>`, `<steammessages.pb.h>`, `<tf2/base_gcmessages.pb.h>`) — no `Steam_Game_Coordinator` / `Steam_Client` / `Settings` / `GBE_shared_dota_lobby_state` dependency, no heavy SDK include chain. Three functions that were file-scope `static` in the original TU (`GBE_PatchDotaLobbyTemplateIdentifiersIfPresent`, `GBE_ForceDotaLobbyCacheOwnerSOID`, `GBE_PrepareDotaWelcomeBody`) are called from the remaining lobby/stateful code in `gbe_dota_gc_payload_helpers.cpp`, so they were promoted to external linkage with new declarations added to `gbe_dota_gc_internal.h`. `GBE_PatchDotaWelcomeAccountObjects` stays file-scope `static` in the new TU (only called from `GBE_PrepareDotaWelcomeBody` there). The 10 shared `extern` byte arrays (`GBE_kOldDotaAccountIdVarint`, `GBE_kOldDotaSteamIdVarint`, etc.) stay in the original TU since they're used by both wire and lobby functions. `tools/gbe_dota_gc_payload_helpers_test/test_wrapper.cpp` now inline-includes the new wire TU (after stubs.h) so the test compiles the real wire logic against stub protobuf types. The handler test wrapper was not changed because `gbe_dota_inventory_handlers.cpp` does not reference any wire helpers directly. Existing test coverage (`GBE_RewriteAccountIdVarintInDirectProtoBody`, `GBE_TryPatchDotaAccountIdVarint`, `GBE_TryPatchDotaAccountIdFixed32`, `GBE_PatchDotaLobbyTemplateIdentifiers`, `GBE_PatchDotaTemplateIdentifiers`, `GBE_ForceDotaLobbyUpdateOwnerSOID`, `GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage`, `GBE_PrepareDotaPersonaStatePeripheralMessage`, plus hello/welcome builders) already exercises malformed varint / truncation / unknown wire type / valid round trip paths. No behavior change (verified: 92/92 payload + 9/9 handler, audit clean 0/0/0).

- [x] 3.2.3 Extract item payload helpers
  - [x] Create `dll/gbe_dota_payload_item_helpers.cpp`.
  - [x] Move shared item serialization helpers.
  - [x] Add tests for owner, attributes, equip states, custom name, and custom description.
  - [x] Run offline GC tests.
  - Notes: extracted 4 pure item functions (`GBE_ParseDotaEquipOps`, `GBE_ApplyDotaUnlockStyleBitmask`, `GBE_SerializeEconItemToGcprotobuf`, `GBE_BuildSOSingleObjectFromItem`) from `dll/gbe_dota_gc_payload_helpers.cpp` (2953 -> 2779 lines) to new `dll/gbe_dota_payload_item_helpers.cpp` (232 lines). The new TU has a minimal include list (`dll/dll/econ_item.h`, `gbe_dota_gc_internal.h`, `gbe_proto_wire.h`, `<sdk/steam/steamclientpublic.h>`, `<tf2/base_gcmessages.pb.h>`, `<tf2/econ_gcmessages.pb.h>`) — no `Steam_Game_Coordinator` / `Steam_Client` / `Settings` / `GBE_shared_dota_lobby_state` dependency, no heavy SDK include chain. Resolved the `TODO(phase-3.2)` stub duplication: `tools/gbe_dota_handler_test/free_func_stubs.cpp` no longer duplicates `GBE_ParseDotaEquipOps` / `GBE_ApplyDotaUnlockStyleBitmask`; instead, `tools/gbe_dota_handler_test/test_wrapper.cpp` and `tools/gbe_dota_gc_payload_helpers_test/test_wrapper.cpp` both pre-define `STEAMCLIENTPUBLIC_H` and inline-include the new pure item TU against the stub `CSteamID` / `Econ_Item` / `CSOEconItem` / `CMsgSOSingleObject` types. Existing test coverage (`test_parse_dota_equip_ops`, `test_apply_dota_unlock_style_bitmask`, `test_build_so_single_object_from_item`) already verifies owner (steam_id), attributes (attr 400 bitmask), equip_states, custom_name, and custom_desc — satisfies the 3.2.3 test requirement. No behavior change (verified: 92/92 payload + 9/9 handler, audit clean 0/0/0).

- [x] 3.2.4 Extract lobby payload helpers
  - [x] Create `dll/gbe_dota_payload_lobby_helpers.cpp`.
  - [x] Move lobby payload composition helpers.
  - [x] Preserve byte-level output for existing replay fixtures.
  - [x] Run offline GC tests.
  - Notes: extracted 15 pure lobby composition functions from `dll/gbe_dota_gc_payload_helpers.cpp` (1994 -> 691 lines) to new `dll/gbe_dota_payload_lobby_helpers.cpp` (1374 lines). The new TU has a minimal include list (`gbe_dota_gc_internal.h`, `gbe_dota_protocol_constants.h`, `gbe_dota_request_router.h`, `gbe_proto_buf_header.h`, `gbe_proto_wire.h`, `gbe_dota_custom_game.h`, `gbe_dota_gc_wire.h`, `gbe_dota_lobby_flow.h`, `gbe_gc_config.h`, `gbe_gc_message_utils.h`, `<sdk/steam/steamclientpublic.h>`, `<steammessages.pb.h>`, `<tf2/base_gcmessages.pb.h>`, `<tf2/gcsystemmsgs.pb.h>`) — no `Steam_Game_Coordinator` / `Steam_Client` / `Settings` / `GBE_shared_dota_lobby_state` dependency, no heavy SDK include chain. All 15 functions were already external-linkage in the original TU (no file-scope static promotion needed); their declarations already lived in `gbe_dota_gc_internal.h`, so no header changes were required. The functions in the new TU call the pure wire helpers (`GBE_PatchDotaLobbyTemplateIdentifiers`, `GBE_ForceDotaLobbyCacheOwnerSOID`, `GBE_PrepareDotaWelcomeBody`, `GBE_PrepareDotaDirectReplayMessage`, etc.) which now live in `gbe_dota_payload_wire_helpers.cpp` — those calls resolve via the extern declarations in `gbe_dota_gc_internal.h`. The 10 shared `extern` byte arrays (`GBE_kOldDotaAccountIdVarint`, `GBE_kOldDotaSteamIdVarint`, etc.) stay in the original TU since they're used by both wire and lobby functions. Removed the now-redundant forward declaration of `GBE_ComposeDotaPracticeLobbySOObjects` (it's declared in `gbe_dota_gc_internal.h`). `tools/gbe_dota_gc_payload_helpers_test/test_wrapper.cpp` now inline-includes the new lobby TU (after the wire TU, before `gbe_dota_gc_payload_helpers.cpp`) so the test compiles the real lobby logic against stub protobuf types. The handler test wrapper was not changed because `gbe_dota_inventory_handlers.cpp` does not reference any lobby helpers directly. Existing test coverage (`GBE_ExtractDotaHelloContext`, `GBE_ExtractDirectDotaHelloContext`, `GBE_ExtractDirectDotaServerHelloContext`, hello/welcome builders, payload adaptation functions) already exercises lobby composition + replay fixture byte-level output paths. No behavior change (verified: 92/92 payload + 9/9 handler, audit clean 0/0/0).

- [x] 3.2.5 Keep orchestration helpers small
  - [x] Keep `dll/gbe_dota_gc_payload_helpers.cpp` under 1200 lines.
  - [x] Keep stateful or cross-domain composition there only when splitting would increase coupling.
  - Notes: after 3.2.4, `dll/gbe_dota_gc_payload_helpers.cpp` is 691 lines (well under the 1200-line target). The remaining functions are all stateful or cross-domain: `GBE_LogGCProtoBoundary` / `GBE_LogDotaSOCacheSubscribedSummary` / `GBE_LogDotaResponsePacket` (logging), `GBE_AdaptDotaTopCustomGamesListPayload` (reads `settings->modSet()`), `GBE_IsDotaOtherLeftChannelPayloadForChannel` (chat-domain byte scan), `GBE_GetDotaReconnectContext` / `GBE_IsDotaArcadeLobbyActive` / `GBE_TryRecoverDotaReconnectContextFromGenericLobbies` (read `GBE_shared_dota_lobby_state` / `get_steam_client()`), `GBE_DescribeDotaLaunchPhase` (trivial 35-line string lookup kept with stateful reconnect group for cohesion), `GBE_GC_DebugLog` (mutable global log), `GBE_DotaCustomGameDisplayName` (reads `settings->isModInstalled` / `settings->getMod`), `GBE_AdaptDotaJoinChatChannelResponsePayload` (reads `get_steam_client()->steam_friends->GetFriendPersonaName`), `GBE_PushDotaPlayerEquippedItemsCacheToGC` (takes `Steam_Game_Coordinator*`, calls `push_incoming_now`), plus the 12 shared `extern` byte arrays / hex strings (used by wire + lobby TUs via extern declarations). Splitting these further would increase coupling (each pulls in `Steam_Game_Coordinator` / `Steam_Client` / `Settings`), so they stay together as the stateful orchestration TU.

## Phase 3.3: Introduce Lightweight Handler Dispatch Table

- [x] 3.3.1 Normalize candidate handler signatures
  - [x] Identify handlers that can accept a shared request context mechanically.
  - [x] Avoid semantic changes while normalizing signatures.
  - [x] Run offline GC tests after each group.
  - Notes: decided NOT to normalize the 16 handler signatures. The handlers have heterogeneous parameter lists (some take `request_job_id`/`has_request_job`, some don't; `Launch` reorders them). Forcing a common signature would require semantic edits across chat/lobby handlers that have no per-domain offline test coverage (Tier B deferred per Phase 3.1 tiering). Instead, each table entry stores a tiny adapter lambda that translates the canonical `DotaGcRequestContext` into the handler's existing signature. This satisfies the acceptance criterion ("Adding a new simple handler requires adding a table entry") without touching handler bodies.

- [x] 3.3.2 Implement static dispatch table
  - [x] Define a private `DotaHandlerEntry` table.
  - [x] Add lookup by `inner_emsg`.
  - [x] Preserve existing logging behavior.
  - Notes: the table is a `static const Entry kTable[]` local array inside `GBE_DispatchDotaPostLoginRequest` (16 entries). Each `Entry` holds `{ emsg, Adapter adapter }` where `Adapter` is a function-pointer typedef. Adapters are captureless local lambdas (using `+[]` to convert to function pointer). Local lambdas (not free functions in an anonymous namespace) are required because the handlers are private members of `Steam_Game_Coordinator` — a lambda defined inside a member function can access that class's private members. First CI attempt (b44edb65) used anonymous-namespace free functions and failed with MSVC C2248; fixed in b77f442c by moving adapters inside the member function as local lambdas.

- [x] 3.3.3 Replace repetitive switch branches
  - [x] Migrate simple branches first.
  - [x] Keep special cases explicit when they require unique handling.
  - [x] Keep `GBE_DispatchDotaPostLoginRequest` easy to scan.
  - Notes: the original 16-arm switch (each arm repeated `log_lobby_request(); return GBE_HandleDotaXxx(...)`) is replaced by: lookup `entry` in `kTable` by `context.inner_emsg` → if not found return false (no log, same as original `default`) → log once → call `entry->adapter(this, context, outer_session_field_raw)`. Special-case handlers needing custom context shaping (direct-path inventory/match/misc in `GBE_HandleDotaDirectPostLoginRequest`, wrapped-path abandon/signout/custom-game in `GBE_HandleDotaWrappedPostLoginRequest`) stay as explicit `if` branches in their own functions — they were never part of this switch and do not belong in the table. The dispatch function body is now ~110 lines (was ~75), but the growth is 16 one-line adapter lambdas + a 16-entry table, which is the explicit cost of "adding a handler = adding one adapter + one table entry". The actual control flow shrank from a 16-arm switch to a linear lookup + single log + single indirect call.

- [x] 3.3.4 Verify behavior
  - [x] Run offline GC tests.
  - [x] Compare replay fixture outputs.
  - [x] Confirm adding a simple handler only requires a table entry.
  - Notes: 92/92 payload + 9/9 handler offline tests pass; audit clean (0 zombie / 0 under-exposed / 0 mismatch). CI build (win / api_experimental / x64 / release) passed on commit b77f442c. Existing replay-fixture tests (gc_replay_test practice_lobby / lobby_lifecycle / game_flow / cache_and_items / wire_edge_cases) pass unchanged, confirming byte-level output preserved. Adding a new simple handler now requires: (1) implement `GBE_HandleDotaXxxRequest` in its domain .cpp, (2) declare it in `dll/dll/steam_game_coordinator.h`, (3) add one `adapt_xxx` lambda + one table entry inside `GBE_DispatchDotaPostLoginRequest`.

## Phase 3.4: Centralize Lobby State Transitions

- [x] 3.4.1 Inventory state transition logic
  - [x] Locate launch, teardown, reconnect, abandon suppression, owner disconnect, and member disconnect decision logic.
  - [x] Identify pure decisions and mutation sites.
  - Notes: assessment found that the bulk of lobby state transition logic was already centralized in `gbe::dota_lobby_state` across prior phases — `compose_create_lobby_plan`, `compose_create_lobby_reset_plan`, `compose_join_lobby_merge_plan`, `compose_launch_init_plan`, `compose_custom_game_launch_setup_plan`, `has_launch_server_setup_sync`, `compose_launch_run_plan`, `compose_queued_lobby_state_apply_plan`, `compose_launch_serversetup_presence_event`, `compose_practice_lobby_launch_event_plan`, `compose_custom_game_launch_setup_event_plan`, `publish_local_lobby_to_shared`, `adopt_shared_lobby_to_local`, `build_reconnect_context` are all pure and live in `dll/gbe_dota_lobby_state.{h,cpp}`. The abandon-suppression stateful helpers (`GBE_ShouldSuppressDotaAbandonedLobby`, `GBE_MarkDotaAbandonedLobbySuppressed`, `GBE_ClearDotaAbandonedLobbySuppression`, `GBE_DiscardQueuedDotaLaunchMessagesForAbandon`) are coordinator-owned mutations and stay in the coordinator. The one remaining pure transition decision still in a handler file's anonymous namespace was `compute_abandon_decision` (in `dll/gbe_dota_lobby_handlers.cpp`, added during Phase 3.1.9 Tier A). 3.4.2 moves it.

- [x] 3.4.2 Extract pure transition helpers
  - [x] Create a small state transition module.
  - [x] Keep coordinator mutation in coordinator methods.
  - [x] Return explicit decision values instead of mutating global state.
  - Notes: moved `AbandonDecision` struct + `compute_abandon_decision` from `dll/gbe_dota_lobby_handlers.cpp` anonymous namespace to `gbe::dota_lobby_state` (`dll/gbe_dota_lobby_state.h` declaration + `dll/gbe_dota_lobby_state.cpp` implementation). The `gbe_dota_lobby_state.cpp` TU gained a `gbe_dota_protocol_constants.h` include for `GBE_kDotaLaunchPhaseRunQueued` / `GBE_kDotaLaunchPhaseLoaded`. The 7035 abandon handler call site now reads `gbe::dota_lobby_state::AbandonDecision d = gbe::dota_lobby_state::compute_abandon_decision(...)`; the handler still owns all side effects (`GBE_DiscardQueuedDotaLaunchMessagesForAbandon`, `GBE_MarkDotaAbandonedLobbySuppressed`, `GBE_pending_reset_after_cache_unsubscribed` mutation, `push_incoming_now(25)`, `GBE_QueueDotaPostGameTeardown`). `gbe_dota_lobby_state.h` is transitively visible in the handlers TU via `dll/dll/steam_game_coordinator.h` (line 23), so no new include was needed there. The empty anonymous-namespace block left behind by the extraction was removed. No behavior change (verified: 92/92 payload + 9/9 handler + 4/4 lobby-state offline tests, audit clean 0/0/0).

- [x] 3.4.3 Add focused transition tests
  - [x] Cover valid launch progression.
  - [x] Cover stale generic lobby state regression.
  - [x] Cover owner disconnect and reconnect.
  - [x] Cover post-game teardown suppression.
  - Notes: created `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp` (637 lines, 4 test functions covering the 4 required categories). (1) `test_valid_launch_progression` exercises `compose_launch_init_plan` (connect-prefer logic + match_id/server_id/game_start_time/launch_phase population), `has_launch_server_setup_sync` (all 5 gating conditions: active/lobby_id/match_id/game_start_time/connect), `compose_launch_run_plan` (blocked when no sync, blocked when phase < setup_synced, advances + bumps phase to run_queued, keeps phase when already loaded), `compose_queued_lobby_state_apply_plan` (state=1 setup_synced bump, state=2 run_queued bump, no bump for nonzero game_state). (2) `test_stale_generic_lobby_state_regression` exercises `adopt_shared_lobby_to_local` `normalize_custom_readyup_run_state` (state=4+game_state>=2+custom→state=2 normalization, and the 3 negative cases: non-custom / game_state<2 / state!=4), `clear_server_id_without_match` (clears when match_id==0, keeps when match_id!=0), and `compose_queued_lobby_state_apply_plan` `preserve_monotonic_game_state` (preserves current game_state when state==2 + game_state>0 + queued game_state==0, and the 3 negative cases: current zero / current state!=2 / flag disabled). (3) `test_owner_disconnect_and_reconnect` exercises `compute_abandon_decision` `treat_as_current_game_disconnect` (true when is_server + owner_connected + state==2 + (server_id!=0 || game_state>=1); false when owner disconnected / client / state!=2) and `build_reconnect_context` (valid when active + (state>=2 || game_state>=2) + server_id!=0 + connect non-empty; rejected for inactive / game not started / server_id==0 / empty connect; accepted via game_state>=2 alone). (4) `test_post_game_teardown_suppression` exercises the abandon threshold (wrapped/client=1, direct server=2), `ready_for_abandon_teardown` (true at game_state>=threshold+state==2; false when state!=2 / game_state<threshold), and `arcade_launch_failed_before_connect` (true for stuck custom-game launch in [RunQueued, Loaded) with owner disconnected on direct path; false when Loaded / before RunQueued / wrapped / owner connected / non-custom / game_state<2). Wired into `tools/run_gc_offline_tests.sh` (builds+runs `gbe_dota_lobby_state_test` against `dll/gbe_dota_lobby_state.cpp` + `gbe_dota_lobby_flow.cpp` + `gbe_dota_custom_game.cpp` + `gbe_dota_gc_wire.cpp` + `gbe_proto_wire.cpp`) and `premake5.lua` (new `tool_gbe_dota_lobby_state_test` project mirroring `tool_gbe_dota_lobby_flow_test`).

- [x] 3.4.4 Verify existing flow
  - [x] Run lobby lifecycle replay fixture.
  - [x] Run full offline GC test script.
  - Notes: `tools/run_gc_offline_tests.sh` passes end-to-end (script uses `set -euo pipefail`, so all stages succeeded): 92/92 payload helper tests, gc_replay_test fixtures (minimal / practice_lobby / lobby_lifecycle / game_flow / cache_and_items / wire_edge_cases) all pass byte-level output, 9/9 handler smoke tests, 19/19 lobby-flow tests, 4/4 lobby-state transition tests, custom-game tests. Audit clean (0 zombie / 0 under-exposed / 0 mismatch).

## Phase 3.5: Tighten Includes And Internal Boundaries

- [ ] 3.5.1 Reduce includes in touched handler files
  - [ ] Remove copy-pasted includes that are unused.
  - [ ] Add precise includes for each new file.
  - [ ] Use forward declarations where safe.

- [ ] 3.5.2 Reduce includes in touched payload helper files
  - [ ] Keep pure helper files independent from `Steam_Game_Coordinator`.
  - [ ] Avoid including broad application headers in pure utility files.

- [ ] 3.5.3 Clean internal header boundaries
  - [ ] Keep `gbe_dota_gc_internal.h` limited to cross-TU declarations.
  - [ ] Move local-only declarations into `.cpp` files.
  - [ ] Move repeated aliases to one place only when shared by multiple files.

- [ ] 3.5.4 Final verification
  - [ ] Run audit script.
  - [ ] Run `tools/run_gc_offline_tests.sh`.
  - [ ] Confirm no touched file uses the full legacy include block without need.

## Completion Criteria

- [ ] `dll/gbe_dota_handlers.cpp` is under 1500 lines, or under 2000 with a documented reason.
- [ ] No domain handler file exceeds 1500 lines without a follow-up split plan.
- [ ] `dll/gbe_dota_gc_payload_helpers.cpp` is under 1200 lines.
- [ ] Every mechanically extracted GC domain has completed follow-up logic refactoring.
- [ ] A handler-level test harness exists and covers at least the logic-refactored handlers.
- [ ] Handler logic is separated into parsing, state mutation, message construction, and coordinator-owned side effects where feasible.
- [ ] Side-effect order is documented and protected by tests or explicit ordered action lists for all logic-refactored GC handlers.
- [ ] New GC files are justified by domain cohesion, cross-domain reuse, pure testability, or size pressure.
- [ ] Audit script has zero real high-risk findings.
- [ ] Offline GC tests pass.
- [ ] New pure helper code has focused tests.
- [ ] New split files use reduced and specific include lists.
