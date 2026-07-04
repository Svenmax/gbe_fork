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
  - [x] Create `dll/gbe_dota_custom_game_handlers.cpp` for the 7070/8052/8053 custom-game loading flow (sub-batch of 3.1.5).
  - [ ] Create `dll/gbe_dota_match_handlers.cpp` when there is a coherent match group.
  - [ ] Create `dll/gbe_dota_misc_handlers.cpp` for remaining one-off handlers.
  - [ ] Keep any shared helper in the smallest reasonable file.
  - [ ] Add follow-up logic refactor tasks for custom-game, match, and misc buckets before marking them complete.
  - [ ] Run offline GC tests.
  - Notes (custom-game sub-batch): moved 3 handlers (`GBE_HandleDotaCustomGameReadyUpRequest`, `GBE_HandleDotaCustomGameStartedLoadingRequest`, `GBE_HandleDotaCustomGameFinishedLoadingRequest`). List X = 0 (no statics), List Y = 0 (no externalize needed; only `GBE_Dota8053Result` alias repeated in new TU). `dll/gbe_dota_handlers.cpp` 3578 → 3459 lines, new file 187 lines. Match and misc sub-buckets still pending. Follow-up logic refactor tracked as 3.1.10.

- [ ] 3.1.6 Reduce or remove original handler file
  - [ ] Keep `dll/gbe_dota_handlers.cpp` under 1000 lines.
  - [ ] Remove it only if no coherent shared content remains.
  - [ ] Verify the build still picks up all new translation units.

- [ ] 3.1.7 Refactor inventory handler logic
  - [ ] Document current side-effect order for all three inventory handlers before changing their internals.
  - [ ] Split `GBE_HandleDotaUnlockItemStyleRequest` into request parsing, unlock mutation, consumable mutation, SO destroy/update construction, response construction, and coordinator side effects.
  - [ ] Split `GBE_HandleDotaSetItemStyleRequest` into request parsing, item style mutation, persistence decision, response construction, and server-GC forwarding decision.
  - [ ] Split `GBE_HandleDotaEquipItemsRequest` into equip-op parsing, inventory mutation, modified-item collection, SO message construction, local response construction, server-GC forwarding, network broadcast, and lobby snapshot refresh decision.
  - [ ] Model side effects as an ordered action list where feasible, then execute the list from the coordinator method.
  - [ ] Keep pure mutation and message-construction helpers independent from `Steam_Game_Coordinator` where feasible.
  - [ ] Keep coordinator methods responsible for owning side effects: `push_incoming_now`, `save_items_to_file`, server-GC calls, network broadcast, and lobby snapshot replay.
  - [ ] Add focused tests for unlock style success, invalid style index, consumable deletion, set style persistence, equip-op mutation, response payload shape, and action ordering.
  - [ ] Run audit script and offline GC tests.

- [ ] 3.1.8 Refactor chat handler logic after extraction
  - [ ] Document current chat side-effect order before changing handler internals.
  - [ ] Split request parsing, channel lookup/mutation, response construction, broadcast construction, and coordinator side effects.
  - [ ] Model side effects as an ordered action list where feasible, then execute the list from the coordinator method.
  - [ ] Move pure chat payload construction into a small helper file when reused by multiple chat handlers.
  - [ ] Add focused tests for join, leave, duplicate join, missing channel, broadcast payload shape, and action ordering.
  - [ ] Run audit script and offline GC tests.

- [ ] 3.1.9 Refactor lobby handler logic after extraction
  - [ ] Document current lobby side-effect order before changing handler internals.
  - [ ] Split request parsing, lobby state mutation, lobby snapshot construction, response construction, invite/kick decisions, and coordinator side effects.
  - [ ] Model side effects as an ordered action list where feasible, then execute the list from the coordinator method.
  - [ ] Keep lobby decision helpers pure where they can return explicit decisions instead of mutating global state.
  - [ ] Add focused tests for create, join, leave, set-details, invite, kick, member state updates, snapshot output stability, and action ordering.
  - [ ] Run audit script and offline GC tests.

- [ ] 3.1.10 Refactor match and misc handler logic after extraction
  - [ ] Document current match/misc side-effect order before changing handler internals.
  - [ ] Split request parsing, state decisions, response construction, and side effects for every moved match/misc handler.
  - [ ] Model side effects as an ordered action list where feasible, then execute the list from the coordinator method.
  - [ ] Group shared pure helpers by actual reuse, not by convenience.
  - [ ] Add focused tests for each handler group that changes logic boundaries, including action ordering for side-effectful paths.
  - [ ] Run audit script and offline GC tests.

- [ ] 3.1.11 Domain bucket completion gate
  - [ ] Confirm every extracted GC handler bucket has a matching logic refactor task completed.
  - [ ] Confirm no bucket is accepted as complete based on mechanical file movement alone.
  - [ ] Confirm each bucket has focused tests or a documented reason why existing replay/offline fixtures are the strongest available coverage.
  - [ ] Confirm side-effect order is documented and covered by tests or explicit ordered action lists for every side-effectful refactor.

- [ ] 3.1.12 Side-effect action model
  - [ ] Define a minimal local action type only when a handler has multiple observable side effects.
  - [ ] Include action type, emsg, payload, target, job id, and reason fields only when needed by tests or execution.
  - [ ] Keep action execution in coordinator-owned code.
  - [ ] Preserve protocol-sensitive order such as `CacheSubscribed` before `emsg21/26`, SO update before response, and full item cache before create/update.
  - [ ] Add action-order assertions before changing legacy side-effect sequences.

- [ ] 3.1.13 Split-boundary discipline
  - [ ] Keep handler buckets aligned to stable domains: inventory, chat, lobby, match, and misc.
  - [ ] Keep local handler-only helpers inside the domain `.cpp` anonymous namespace.
  - [ ] Create a separate helper file only for cross-domain reuse, pure testable logic, or an oversized domain file.
  - [ ] Treat handler bucket files around `300-1200` lines as healthy when responsibilities remain cohesive.
  - [ ] Treat pure helper files around `200-1000` lines as healthy when responsibilities remain cohesive.
  - [ ] Require at least two real call sites before moving a helper into a shared cross-file helper module.
  - [ ] Document the responsibility boundary and migration reason for every new GC `.cpp` file.

## Phase 3.2: Separate Payload Helpers

- [ ] 3.2.1 Inventory payload helper inventory
  - [ ] List pure wire parsing functions.
  - [ ] List pure item serialization functions.
  - [ ] List lobby payload composition functions.
  - [ ] List functions that depend on coordinator or global state.

- [ ] 3.2.2 Extract pure wire helpers
  - [ ] Create `dll/gbe_dota_payload_wire_helpers.cpp` and matching internal header if needed.
  - [ ] Move pure parse/build helpers with no coordinator dependency.
  - [ ] Add tests for malformed varint, truncation, unknown wire type, and valid round trip.
  - [ ] Run offline GC tests.

- [ ] 3.2.3 Extract item payload helpers
  - [ ] Create `dll/gbe_dota_payload_item_helpers.cpp`.
  - [ ] Move shared item serialization helpers.
  - [ ] Add tests for owner, attributes, equip states, custom name, and custom description.
  - [ ] Run offline GC tests.

- [ ] 3.2.4 Extract lobby payload helpers
  - [ ] Create `dll/gbe_dota_payload_lobby_helpers.cpp`.
  - [ ] Move lobby payload composition helpers.
  - [ ] Preserve byte-level output for existing replay fixtures.
  - [ ] Run offline GC tests.

- [ ] 3.2.5 Keep orchestration helpers small
  - [ ] Keep `dll/gbe_dota_gc_payload_helpers.cpp` under 1200 lines.
  - [ ] Keep stateful or cross-domain composition there only when splitting would increase coupling.

## Phase 3.3: Introduce Lightweight Handler Dispatch Table

- [ ] 3.3.1 Normalize candidate handler signatures
  - [ ] Identify handlers that can accept a shared request context mechanically.
  - [ ] Avoid semantic changes while normalizing signatures.
  - [ ] Run offline GC tests after each group.

- [ ] 3.3.2 Implement static dispatch table
  - [ ] Define a private `DotaHandlerEntry` table.
  - [ ] Add lookup by `inner_emsg`.
  - [ ] Preserve existing logging behavior.

- [ ] 3.3.3 Replace repetitive switch branches
  - [ ] Migrate simple branches first.
  - [ ] Keep special cases explicit when they require unique handling.
  - [ ] Keep `GBE_DispatchDotaPostLoginRequest` easy to scan.

- [ ] 3.3.4 Verify behavior
  - [ ] Run offline GC tests.
  - [ ] Compare replay fixture outputs.
  - [ ] Confirm adding a simple handler only requires a table entry.

## Phase 3.4: Centralize Lobby State Transitions

- [ ] 3.4.1 Inventory state transition logic
  - [ ] Locate launch, teardown, reconnect, abandon suppression, owner disconnect, and member disconnect decision logic.
  - [ ] Identify pure decisions and mutation sites.

- [ ] 3.4.2 Extract pure transition helpers
  - [ ] Create a small state transition module.
  - [ ] Keep coordinator mutation in coordinator methods.
  - [ ] Return explicit decision values instead of mutating global state.

- [ ] 3.4.3 Add focused transition tests
  - [ ] Cover valid launch progression.
  - [ ] Cover stale generic lobby state regression.
  - [ ] Cover owner disconnect and reconnect.
  - [ ] Cover post-game teardown suppression.

- [ ] 3.4.4 Verify existing flow
  - [ ] Run lobby lifecycle replay fixture.
  - [ ] Run full offline GC test script.

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

- [ ] `dll/gbe_dota_handlers.cpp` is under 1000 lines or removed.
- [ ] `dll/gbe_dota_gc_payload_helpers.cpp` is under 1200 lines.
- [ ] Every mechanically extracted GC domain has completed follow-up logic refactoring.
- [ ] Handler logic is separated into parsing, state mutation, message construction, and coordinator-owned side effects where feasible.
- [ ] Side-effect order is documented and protected by tests or explicit ordered action lists for all logic-refactored GC handlers.
- [ ] New GC files are justified by domain cohesion, cross-domain reuse, pure testability, or size pressure.
- [ ] Audit script has zero real high-risk findings.
- [ ] Offline GC tests pass.
- [ ] New pure helper code has focused tests.
- [ ] New split files use reduced and specific include lists.
