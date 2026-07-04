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

- [ ] 3.1.3 Extract chat handlers
  - [ ] Create `dll/gbe_dota_chat_handlers.cpp`.
  - [ ] Move chat join, leave, and message handlers.
  - [ ] Move chat-only constants and helpers.
  - [ ] Run offline GC tests.

- [ ] 3.1.4 Extract lobby handlers
  - [ ] Create `dll/gbe_dota_lobby_handlers.cpp`.
  - [ ] Move lobby create, join, leave, list, invite, kick, and set-details handlers.
  - [ ] Keep signatures unchanged.
  - [ ] Run offline GC tests.

- [ ] 3.1.5 Extract match and misc handlers
  - [ ] Create `dll/gbe_dota_match_handlers.cpp` when there is a coherent match group.
  - [ ] Create `dll/gbe_dota_misc_handlers.cpp` for remaining one-off handlers.
  - [ ] Keep any shared helper in the smallest reasonable file.
  - [ ] Run offline GC tests.

- [ ] 3.1.6 Reduce or remove original handler file
  - [ ] Keep `dll/gbe_dota_handlers.cpp` under 1000 lines.
  - [ ] Remove it only if no coherent shared content remains.
  - [ ] Verify the build still picks up all new translation units.

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
- [ ] Audit script has zero real high-risk findings.
- [ ] Offline GC tests pass.
- [ ] New pure helper code has focused tests.
- [ ] New split files use reduced and specific include lists.
