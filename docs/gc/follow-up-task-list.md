# Dota GC Follow-up Task List

This is the executable follow-up checklist for Dota GC maintenance work after the coordinator refactor. It should stay practical: every task needs a clear behavior goal, a test/verification expectation, and a stop condition.

Reference docs:

- `future-refactor-plan.md`: overall sequence and stop conditions.
- `shared-lobby-state-contract.md`: `GBE_shared_dota_lobby_state` contract and facade rules.
- `lifecycle-state-map.md`: lifecycle paths, risks, and state transitions.
- `test-coverage-map.md`: which offline test to extend for each kind of change.

## P0. Before More Refactor

Do these before broad shared-state or side-effect refactors.

- [x] Add a focused test proving a host client observing postgame does not clear shared state still needed by the server GC.
  - Goal: protect the server/client ownership boundary before lifecycle cleanup helpers grow.
  - Suggested place: `tools/gbe_dota_handler_test/smoke_test.cpp` or a focused lifecycle test if production linkage becomes practical.
  - Verify: `bash tools/run_gc_verification.sh --full`.
  - Result: added `compute_postgame_observation_decision(...)` and focused lobby-state coverage for host-client skip, host-client-over-arcade precedence, ordinary player cleanup, arcade-active skip, server path, non-transition, and zero-lobby-id cases. Production postgame observation now consumes the same pure decision without changing side-effect order.
  - Verified: `bash tools/run_gc_verification.sh --full` passed; handler smoke tests later expanded to `43/43` and still passed; audit issues `0`.

- [x] Add a focused test for reconnect preserve-vs-clear behavior.
  - Goal: make it explicit which lifecycle paths preserve reconnect context and which paths clear it.
  - Suggested place: `tools/gbe_dota_gc_payload_helpers_test/gbe_dota_gc_payload_helpers_test.cpp` for payload decisions; handler/lifecycle test for cleanup triggers.
  - Verify: `bash tools/run_gc_verification.sh --full`.
  - Result: added `compute_runtime_reset_decision(...)` and focused lobby-state coverage showing `7035_disconnect_current_game_after_25` preserves reconnect context while shutdown, ordinary leave, and unknown reset reasons clear it. Production `ResetGCMemory(...)` now consumes the same pure decision before calling `clear_dota_runtime_state(...)`.
  - Verified: `bash tools/run_gc_verification.sh --full` passed; handler smoke tests later expanded to `43/43` and still passed; audit issues `0`.

- [x] Add or strengthen a stale postgame chat leave test.
  - Goal: prove a cleared shared state is not republished from stale local lobby state.
  - Suggested place: adjacent to `test_chat_leave_postgame_channel_order`.
  - Verify: `bash tools/run_gc_verification.sh --full`.
  - Result: added `test_chat_leave_postgame_skips_stale_republish_after_shared_clear`, covering the branch where postgame leave still returns 7014 while cleared shared state remains invalid and stale local lobby state is cleared instead of republished.
  - Verified: `bash tools/run_gc_verification.sh --full` passed; handler smoke tests later expanded to `43/43` and still passed; audit issues `0`.

- [x] Decide whether the reset contract needs a production-linked focused test.
  - Current state: `test_lobby_runtime_reset_clears_local_shared_and_last_launch_state` protects the handler-test stub contract, while the production helper is small and behavior-equivalent.
  - Goal: avoid overbuilding unless future reset helpers become less trivial.
  - Stop condition: do not add heavy linkage just to satisfy aesthetics.
  - Decision: do not add a production-linked focused test now. The production `GBE_ClearDotaLobbyRuntimeState()` and handler-test stub both clear `GBE_local_lobby`, `GBE_shared_dota_lobby_state`, and last pushed launch game state with no branching. A production-linked test would add linkage complexity without improving current lifecycle confidence.
  - Revisit if the helper gains branching, extra side effects, or a semantic wrapper replaces direct reset calls.

## P1. Shared Lobby State Read-Only Facade

Start with read-only access. Do not change publish or clear behavior in this phase.

- [x] Add one or more read-only helpers.
  - Candidate helpers: `GBE_HasSharedDotaLobbyState()`, `GBE_GetSharedDotaLobbyStateSnapshot()`, `GBE_GetSharedDotaLobbyIdOrZero()`, `GBE_IsSharedDotaLobbyActive()`.
  - Rule: do not return a mutable reference.
  - Result: added `GBE_IsSharedDotaArcadeLobbyActive()` as a narrow read-only helper for the existing arcade-active predicate, and `GBE_GetSharedDotaReconnectStateSnapshot()` for reconnect-only shared-state reads.

- [x] Add focused tests for helper equivalence.
  - Goal: direct-field reads and helper reads produce the same decisions.
  - Suggested place: payload helper tests for reconnect/read decisions; handler smoke tests only when side effects matter.
  - Result: extended `test_is_dota_arcade_lobby_active` to assert the new helper and the existing `GBE_IsDotaArcadeLobbyActive()` stay equivalent across empty, non-arcade, active arcade, and inactive states. Extended `test_get_dota_reconnect_context` to assert the reconnect snapshot matches the direct shared-state fields used by reconnect eligibility.

- [x] Replace one small cluster of obviously read-only call sites.
  - Good first target: payload helper or decision code that only reads validity/lobby id/active state.
  - Bad first target: publish, clear, postgame cleanup, or mixed mutation paths.
  - Result: replaced `GBE_IsDotaArcadeLobbyActive()` internals with the new read-only helper, and replaced `GBE_GetDotaReconnectContext()` shared-state reads with the reconnect snapshot helper.

- [x] Run verification.
  - Required: `bash tools/run_gc_verification.sh --full`.
  - Required: `git diff --check`.
  - Verified: `bash tools/run_gc_verification.sh --full` passed; payload helper tests later expanded to `187/187` and still passed; handler smoke tests later expanded to `43/43` and still passed; audit issues `0`.

- [x] Stop if review gets harder.
  - If call chains become longer without clearer lifecycle intent, revert or shrink the step.
  - Decision: stopped after the narrow arcade-active and reconnect snapshot helpers. Broader shared-state reads should still be replaced only one tested read-only cluster at a time.

## P1. Lifecycle Helpers And Full Reset Semantics

Only add helpers that name real lifecycle intent. Keep message order stable.

- [x] Keep `GBE_ClearDotaLobbyRuntimeState()` as the full local/shared/last-launch reset helper.
  - Goal: preserve the existing full-reset meaning.
  - Decision: keep the existing helper as the full local/shared/last-launch reset helper. It remains the right boundary for paths that intentionally clear `GBE_local_lobby`, `GBE_shared_dota_lobby_state`, and last pushed launch game state together.

- [x] Consider a normal-signout semantic helper only after tests are in place.
  - Candidate name: `GBE_ClearDotaRuntimeForNormalSignout(...)`.
  - Must preserve cache-unsubscribe ordering.
  - Decision: do not add this helper now. `GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(...)` already names the lifecycle intent and preserves the cache-unsubscribe boundary around the existing full reset.

- [x] Consider a postgame cleanup semantic helper only after tests are in place.
  - Candidate name: `GBE_FinalizeDotaPostgameCleanup(...)`.
  - Must preserve host-client/server-GC ownership behavior.
  - Decision: do not add this helper now. The host-client/server-GC ownership boundary is protected by `compute_postgame_observation_decision(...)`, and wrapping the cleanup sequence would add indirection around message ordering.

- [x] Keep partial cleanup exceptions explicit.
  - Example: stale postgame chat leave can clear local state without broad runtime reset.
  - Rule: do not fold exceptions into generic helpers unless tests prove the behavior remains identical.
  - Decision: keep the stale postgame chat leave exception at the call site. `test_chat_leave_postgame_skips_stale_republish_after_shared_clear` now protects the local-only cleanup behavior.

## P1. Side-Effect Recorder Coverage

Before wrapping more side effects, make ordering assertions stronger.

- [x] Extend handler recorder coverage for one high-risk path at a time.
  - Candidate paths: normal signout, postgame cleanup, lobby destroy, stale chat leave, set-details publish/update.
  - Result: extended stale postgame chat leave recorder coverage by recording `GenericLobbyLeave` in the handler harness.

- [x] Assert ordering, not just presence.
  - Examples: cache unsubscribe before cleanup, mutation before publish, 7014 before postgame local cleanup, details update after publish.
  - Result: `test_chat_leave_postgame_skips_stale_republish_after_shared_clear` now asserts `7014` is pushed before generic/local cleanup and still verifies no stale shared-state publish occurs.

- [x] Document any reason-string changes.
  - If a high-risk reason string changes or is added, update `reason-trace-governance.md` and keep `_audit_gc_refactor.py` green.
  - Result: no reason strings changed or added. `bash tools/run_gc_verification.sh --full` passed with audit issues `0`.

## P2. Shared Lobby State Clear Facade

Only start after read-only facade work is stable.

- [x] Add behavior-equivalent clear helper.
  - First implementation should be exactly equivalent to assigning `GBE_SharedDotaLobbyState{}`.
  - Do not add preserve logic in the first clear-helper PR.
  - Result: added `GBE_ClearSharedDotaLobbyState()` with behavior exactly equivalent to `GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{}`.

- [x] Replace one direct clear site.
  - Choose the least ambiguous full-clear path first.
  - Avoid postgame/server-client ownership paths until tests are stronger.
  - Result: replaced the single shared-state clear inside `GBE_ClearDotaLobbyRuntimeState()`. Postgame/server-client ownership paths remain unchanged.

- [x] Verify no stale re-publish behavior changed.
  - Run full verification.
  - Add focused test if existing coverage is indirect.
  - Verified: `bash tools/run_gc_verification.sh --full` passed; `test_chat_leave_postgame_skips_stale_republish_after_shared_clear` still covers stale republish prevention; audit issues `0`.

- [x] Only later split clear helpers by lifecycle intent.
  - Candidate intents: normal signout, player postgame cleanup, full runtime reset.
  - Decision: keep lifecycle-specific clear helpers deferred. The first clear facade only names the raw shared-state reset operation and does not add preserve or lifecycle semantics.

## P2. Dependency Seams

Centralize dependencies only when doing so improves review or testability.

- [x] Add read-only wrappers for settings/lobby lookups where repeated and risky.
  - Candidate: host/server lobby ownership checks.
  - Result: added `GBE_HostHasActiveDotaServerLobby(...)` as a behavior-equivalent read-only seam for the postgame host-client/server-GC ownership check. Added `test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state`, `test_lobby_player_postgame_observation_clears_shared_state_after_details_update`, `test_lobby_arcade_active_postgame_observation_preserves_shared_state`, and `test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip` with a one-shot capture override in the handler harness so the tests exercise host-client, ordinary player, arcade-active, and host-over-arcade precedence outcomes while preserving active arcade runtime details-update suppression without broad generic-lobby metadata simulation.

- [x] Add named helper for settings lobby clear only if ordering remains obvious.
  - Candidate: `GBE_ClearSettingsLobbyForDotaSignout(...)`.
  - Result: added `GBE_ClearSettingsLobbyForDotaSignout()` and replaced three Dota GC settings-lobby clear sites while preserving the existing nonzero check and side-effect order.
  - Follow-up: strengthened recorder coverage so normal signout proves cache unsubscribe happens before settings clear, the cleared settings lobby id matches the consumed pending id, and a second clear is a no-op when settings lobby is already empty.

- [x] Wrap rich-presence operations only after recorder/test coverage can assert ordering.
  - Decision: no rich-presence wrapper added now. Existing recorder coverage still does not assert rich-presence ordering directly, so wrapping would add indirection without stronger tests.

- [x] Keep wrappers behavior-equivalent first.
  - No retry policy, logging policy, or preserve semantics in the first seam step unless already tested.
  - Verified: the new settings-lobby helper is behavior-equivalent and `bash tools/run_gc_verification.sh --full` passed with audit issues `0`.

## P2/P3. Inventory, VPK, And Template Replay

Lower priority unless these areas become active bug sources.

- [x] Keep `GBE_vpk_loot_data` mutation centralized through `GBE_SetDotaVpkLootData(...)`.
  - Decision: no code change needed. `GBE_vpk_loot_data` remains file-local in `steam_game_coordinator.cpp`, with reads through `GBE_GetDotaVpkLootData()` and mutation through `GBE_SetDotaVpkLootData(...)`.

- [x] Add tests before changing inventory/VPK ownership.
  - Suggested tests: handler inventory smoke tests, custom-game tests, replay fixtures when output-visible.
  - Decision: no inventory/VPK ownership change in this pass. Existing handler inventory smoke tests and replay fixtures remain the required baseline before future changes.

- [x] Do not move template blobs for appearance alone.
  - Fixture output must stay stable if blob ownership changes.
  - Decision: no template blobs moved. Existing template ownership remains covered by full verification and the template/replay canned blob ownership audit.

## P3. Dota Sub-Object Extraction Decision

Do not start until shared-state facade, lifecycle tests, and dependency seams are stable.

- [x] Re-evaluate whether a Dota sub-object can avoid depending on the entire `Steam_Game_Coordinator *`.
  - Decision: do not extract a Dota sub-object now. Current Dota handlers and coordinators still rely on a broad coordinator surface: `GBE_local_lobby`, `GBE_shared_dota_lobby_state`, settings, network, push queues, rich presence, and `get_steam_client()`.

- [x] Extract only if the new object has a small explicit context and can be tested directly.
  - Decision: the explicit context is not yet small enough. Continue extracting pure decision helpers and narrow seams first; those are directly testable and have shown better review value.

- [x] Stop if extraction creates several smaller objects that still share the same implicit global state.
  - Decision: stop before extraction. A sub-object today would mostly move member functions while retaining the same implicit global state and side-effect dependencies.

## Next Phase Candidates

The checklist above records the first follow-up pass and includes several "decided not to do now" items. It should not be read as the end of GC maintenance work. Use this section for the next small, reviewable steps.

- [x] Shared lobby state read-only facade phase 2.
  - Goal: replace one more clearly read-only cluster without touching publish or clear behavior.
  - First cluster: shared-state existence and lobby-id fallback reads used by stale postgame leave and invite fallback logic.
  - Required tests: helper equivalence assertions in the payload helper test, plus existing stale postgame leave handler coverage.
  - Result: added `GBE_HasSharedDotaLobbyState()`, `GBE_GetSharedDotaLobbyIdOrZero()`, and `GBE_GetSharedDotaGenericLobbyIdOrZero()`; replaced the stale postgame leave validity check and invite fallback lobby-id reads. No publish, clear, or lifecycle-specific mutation paths were changed.
  - Verified: `bash tools/run_gc_verification.sh --fast` passed with payload helper tests `196/196`, handler smoke tests `43/43`, and audit issues `0`.

- [x] Side-effect recorder coverage phase 2.
  - Goal: strengthen ordering assertions before introducing any broader side-effect interface.
  - Candidate paths: normal signout, lobby destroy, set-details publish/update.
  - Result: extended handler test recorder details-update and runtime-state observations with `action_sequence_index`, then asserted kick/set-details send practice-lobby details updates only after the shared-state publish action has already been recorded, and 7034 connected/disconnected runtime mutations happen before any response action is recorded. No production side-effect wrapper was introduced.
  - Stop condition: do not wrap rich presence or network broadcast until recorder coverage can prove ordering.
  - Verified: `bash tools/run_gc_verification.sh --fast` passed with payload helper tests `196/196`, handler smoke tests `43/43`, and audit issues `0`.

- [x] Side-effect recorder coverage phase 3.
  - Goal: strengthen one more high-risk ordering assertion before broader side-effect wrappers.
  - Result: extended the handler recorder with `SettingsLobbyClear` and strengthened `test_lobby_normal_signout_pending_clear_resets_state` so it records cache unsubscribe before runtime/settings cleanup, verifies local/shared/launch-state reset, and asserts the settings lobby clear happens after cache unsubscribe while preserving the consumed lobby id. No production side-effect wrapper was introduced.
  - Stop condition: do not wrap settings, rich presence, or server/client lookup behavior until recorder coverage proves the affected ordering.

- [x] Shared lobby state read-only facade phase 3.
  - Goal: evaluate another read-only cluster only after phase 2 lands cleanly.
  - Candidate areas: payload/lobby helper reads that can consume snapshots or scalar helpers.
  - Result: added `GBE_GetSharedDotaLobbyScalarSnapshot()` for the read-only scalar cluster `valid/active/lobby_id/generic_lobby_id/state/game_state`, covered invalid and valid helper semantics in payload helper tests, and replaced one launch suppression read with the immutable snapshot. No publish, clear, or lifecycle-specific mutation semantics changed.
  - Follow-up: documented the helper as a raw immutable snapshot and added coverage proving invalid shared state preserves raw ids in the snapshot while valid-gated id helpers still return zero.
  - Follow-up: replaced the queued lobby-state preapply valid/active/lobby-id read cluster with the raw scalar snapshot. The restore condition and no-context debug log remain behavior-equivalent.
  - Stop condition: avoid large snapshot plumbing if direct field reads are still clearer and low risk.

- [x] Shared lobby state clear semantic wrapper prep.
  - Goal: name one lifecycle clear intent after behavior is already covered.
  - Result: added `GBE_ClearSharedDotaLobbyForRuntimeReset()` as a behavior-equivalent wrapper around `GBE_ClearSharedDotaLobbyState()` and replaced only the runtime reset call site. The existing `test_lobby_runtime_reset_clears_local_shared_and_last_launch_state` continues to protect local/shared/last-launch reset behavior.
  - Follow-up: evaluated `GBE_ClearSharedDotaLobbyForPlayerPostgameCleanup()` and stopped before adding it because ordinary player postgame cleanup currently reaches shared clearing through the full runtime reset helper, with no direct shared-clear call site to name safely.
  - Stop condition: no preserve logic, logging, side effects, flags, or server/client ownership changes were added.

- [x] Push / response seam phase 1.
  - Goal: name one repeated side-effect operation only after recorder coverage proves order and metadata.
  - Result: added `GBE_PushDotaCacheUnsubscribedResponse()` as a thin helper over `GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, ...)` and replaced the two 7040 leave-family cache-unsubscribed response call sites. Existing handler coverage verifies emsg `25`, wrapped/session metadata, reason string, and clear-after-response order for `7040_leave_25`.
  - Stop condition: no broad side-effect interface was introduced; immediate-vs-delayed response behavior remains owned by `GBE_PushDotaResponse()`.

- [x] Runtime payload parser edge-case coverage.
  - Goal: add focused fixture coverage for `parse_dota7034_runtime_request` without changing production behavior.
  - Result: extended the pure parser test with empty input and a truncated nested connected-player field, verifying malformed runtime payloads leave connected/disconnected/game-state/draft flags unset.
  - Stop condition: no payload adaptation or handler behavior changed.

- [x] Side-effect recorder coverage phase 4.
  - Goal: strengthen server-GC forward, network broadcast, and lobby snapshot refresh assertions before adding more side-effect seams.
  - Result: extended the equip-items full-forward handler smoke test so cache-forward records the local steam id, equipped-items network broadcast records the local steam id as source, and lobby snapshot refresh proves the `equip_items_refresh` reason after the prior local response/save/server-forward/broadcast actions. No production side-effect wrapper was introduced.
  - Stop condition: do not add a broad side-effect interface until at least the selected family can be reviewed through metadata-rich recorder assertions.

- [x] Push / response seam phase 2.
  - Goal: name one more repeated response operation only where existing recorder coverage proves metadata.
  - Result: added `GBE_PushDotaOtherLeftChannelResponse()` as a thin helper over `GBE_PushDotaResponse(GBE_kDotaOtherLeftChannel, ...)` and replaced the stale and normal `7272` chat leave `7014` response call sites. Existing chat leave coverage verifies emsg `7014`, wrapped/session metadata, reason strings `stale_7272_7014` and `7272_7014`, and response-before-cleanup/publish ordering.
  - Stop condition: no broad response interface was introduced; immediate-vs-delayed response behavior remains owned by `GBE_PushDotaResponse()`.

- [x] Client/ownership seam evaluation.
  - Goal: identify a narrow client coordinator lookup path before adding an ownership helper.
  - Result: identified `GBE_PushDotaLaunchStateToClientPeer(...)` as the candidate path. It selects `steam_game_coordinator` when called from the server coordinator, restores shared lobby state on the target, and pushes launch-state `24`/`26` through the client coordinator.
  - Stop condition: no helper added now because the existing handler smoke harness does not link `gbe_dota_lobby_launch_coordinator.cpp`; adding direct target-selection coverage would broaden the test wrapper surface before a focused launch coordinator harness exists.

- [x] Payload malformed-input coverage phase 2.
  - Goal: grow pure payload fixture coverage without changing handler behavior.
  - Result: extended `GBE_ParseDotaEquipOps` tests with slot-overflow and style-overflow malformed inputs, protecting the uint16 slot narrowing and uint8 style-index narrowing checks.
  - Stop condition: no parser behavior or handler adaptation changed.

- [x] Unlock style bitmask coverage phase 2.
  - Goal: strengthen pure style-index and attr-merge coverage before future inventory refactors.
  - Result: extended `GBE_ApplyDotaUnlockStyleBitmask` tests with highest valid style index `31` and an existing attr-400 entry whose short `value_bytes` are treated as zero before OR-ing the requested style bit.
  - Stop condition: no unlock behavior or inventory handler behavior changed.

- [x] Template identifier output-equivalence coverage.
  - Goal: add binary output-equivalence coverage only where expected bytes can be constructed exactly.
  - Result: extended `GBE_PatchDotaLobbyTemplateIdentifiers` tests with a minimal exact-output fixture that checks the complete patched byte string for lobby-id varint and steam-id fixed64 replacements.
  - Stop condition: no template patch behavior changed; broader canned template fixtures remain deferred unless exact expected output is practical.

- [x] Object extraction readiness checkpoint.
  - Goal: re-evaluate whether Dota sub-object extraction is ready after the current seam and payload coverage pass.
  - Result: `bash tools/run_gc_verification.sh --full` passed with payload helper tests `238/238`, handler smoke tests `43/43`, and audit issues `0`; `git diff --check` passed.
  - Decision: defer Dota sub-object extraction. The shared-state read/clear seams and response seams are stronger, and recorder coverage now spans settings clear, server-GC forward, network broadcast, lobby snapshot refresh, and response metadata. Client/ownership target-selection now has a narrow pure helper seam for launch-state peer selection, but the full launch-state client-peer push path still depends on broad `Steam_Game_Coordinator` state.
  - Stop condition: begin extraction only after the full launch-state client-peer path can be covered through a focused launch coordinator harness or another narrow path with a small explicit context.

- [x] Launch-state peer target-selection seam.
  - Goal: add ownership target-selection coverage without linking the production-only launch coordinator into the handler smoke harness.
  - Result: added `should_use_client_peer_for_launch_state_push`, `is_valid_launch_state_push_target`, and `should_preserve_server_id_for_launch_state_push_target` to `gbe::dota_lobby_flow`, covered server/client peer selection, valid Dota-client target eligibility, and owner-LAN preserve eligibility in `gbe_dota_lobby_flow_test`, and replaced the matching checks in `GBE_PushDotaLaunchStateToClientPeer`.
  - Stop condition: no broad launch coordinator harness added; full push side effects remain behind the existing production coordinator path.

- [x] Focused launch coordinator harness feasibility pass.
  - Goal: determine whether the full launch-state client-peer push path can be covered without broadening the handler smoke harness.
  - Result: the full path still depends on shared-state restore, current-lobby capture, cache-subscribed/details-update builders, cache subscription recording, queue pushes, rich presence, settings local steam id, duplicate suppression, and coordinator private state. The owner-LAN preserve branch was extracted to a pure helper instead.
  - Stop condition: defer a full launch coordinator harness until those dependencies can be represented as a small explicit context or until the launch push is split into a planner with recorded side effects.

- [x] Launch-state push planner pass.
  - Goal: turn the launch-state client-peer push decision path into a pure planner before any harness or object extraction work.
  - Result: added `LaunchStatePushPlanInput`, `LaunchStatePushPlan`, and `plan_launch_state_push` to `gbe::dota_lobby_flow`; covered invalid target, suppressed shared lobby, inactive capture, ineligible launch state, duplicate game state, normal push, connect-endpoint push, and owner-LAN preserve in `gbe_dota_lobby_flow_test`; integrated the planner into `GBE_PushDotaLaunchStateToClientPeer` while preserving restore, capture, build, push, rich-presence, last-state, and debug-log order.
  - Stop condition: full focused launch coordinator harness and Dota sub-object extraction remain deferred because side-effect execution still depends on broad coordinator state.

- [x] Launch-state side-effect order planning pass.
  - Goal: make successful launch-state side-effect order explicit and testable before introducing a production executor.
  - Result: added `LaunchStatePushAction` and `launch_state_push_actions` to `gbe::dota_lobby_flow`, covering record cache subscription, push cache subscribed, push details update, reapply rich presence, and set last game state in production order; skipped plans return an empty action sequence.
  - Stop condition: production executor remains deferred because it would still be a coordinator member wrapper over existing side effects without a narrower harness.

- [x] Rich presence recorder coverage pass.
  - Goal: make rich presence updates visible in handler smoke tests before adding any rich presence wrapper.
  - Result: added `RichPresenceUpdate` to the handler action recorder, strengthened postgame chat leave tests to assert `7014` before rich presence reset and cleanup/publish behavior, and added a 7041 standard launch smoke test proving shared-lobby publish and initial details `26` precede the serversetup rich presence update.
  - Stop condition: no production rich presence wrapper was introduced; this pass only records and asserts existing side-effect order and fields.

- [x] Rich presence server-setup reset seam.
  - Goal: name the repeated Dota rich presence reset intent after recorder coverage proved the affected ordering.
  - Result: added `GBE_ResetDotaPracticeLobbyLaunchRichPresenceToServerSetup()` as a behavior-equivalent wrapper for `#DOTA_RP_INIT` / `SERVERSETUP` with party and lobby fields cleared, then replaced the postgame chat leave and `ResetGCMemory(...)` reset call sites.
  - Stop condition: no new rich presence policy, retry behavior, logging, or ordering changes were added.

- [x] Launch persona-state recorder coverage pass.
  - Goal: make the non-queued persona-state side effect visible in handler smoke tests after rich presence ordering became visible.
  - Result: added `LaunchPersonaState` to the handler action recorder and strengthened the 7041 standard launch smoke test to prove shared-lobby publish, initial details `26`, rich presence update, and persona-state metadata build order, including status, lobby state, party/lobby flags, and reason.
  - Stop condition: no production persona-state behavior changed and no custom-game launch coordinator linkage was added.

- [x] Generic lobby publish recorder coverage pass.
  - Goal: make local member-data and generic lobby metadata publishes visible before wrapping more lobby publish side effects.
  - Result: added `LobbyLocalMemberData` and `LobbyMetadataPublish` to the handler action recorder, strengthened 7046 set-details coverage to assert local member data, shared-state publish, metadata publish, then details update order, and strengthened 8053 finished-loading coverage to assert local member data before shared-state publish.
  - Stop condition: no production publish helper was introduced; this pass only records existing side-effect order and reasons.

- [x] Launch peripheral reset recorder coverage pass.
  - Goal: make launch peripheral reset visible before any launch lifecycle helper grows around 7041.
  - Result: added `LaunchPeripheralReset` to the handler action recorder and strengthened the 7041 standard launch smoke test to prove peripheral reset happens before shared-lobby publish, initial details `26`, rich presence update, and persona-state metadata build.
  - Stop condition: no production reset behavior changed and no launch coordinator linkage was expanded.

- [x] Lobby cache subscription recorder coverage pass.
  - Goal: make cache subscription state recording visible before extracting or wrapping create/join cache-subscription side effects.
  - Result: added `LobbyCacheSubscriptionRecord` to the handler action recorder, added a 7038 create smoke test proving local member data publish, shared lobby publish, metadata publish, cache subscription record, cache subscribed `24`, then practice lobby response `7055` order, and added a 7044 join smoke test proving local member data publish, shared lobby publish, cache subscription record, cache subscribed `24`, then join response `7113` order.
  - Stop condition: no production cache subscription behavior changed and no join/custom-game linkage was expanded.

- [x] Settings lobby sync recorder coverage pass.
  - Goal: make settings-lobby sync visible in the 7038 create lifecycle before adding any wrapper around lobby setup publishing.
  - Result: added `SettingsLobbySync` to the handler action recorder, strengthened the 7038 create smoke test to prove local member data publish, settings sync, shared lobby publish, metadata publish, cache subscription record, cache subscribed `24`, then practice lobby response `7055` order, and added a 7044 matched-generic join smoke test proving settings sync happens before local member data publish, shared lobby publish, cache subscription record, cache subscribed `24`, then join response `7113`.
  - Stop condition: no production settings sync behavior changed and no generic-lobby metadata surface was widened.

- [x] Abandoned lobby suppression recorder coverage pass.
  - Goal: make abandoned-lobby suppression visible before adding lifecycle wrappers around leave/abandon teardown paths.
  - Result: added `AbandonedLobbySuppressed` to the handler action recorder and strengthened current-game disconnect, ordinary leave, and ready-for-abandon teardown smoke tests to prove suppression happens before cache-unsubscribed `25` or postgame `7014` responses with the correct lobby id and reason.
  - Stop condition: no production suppression behavior changed and queued-launch discard remains deferred until a focused launch-failure path can cover it.

- [x] Queued launch discard recorder coverage pass.
  - Goal: make queued launch message discard visible before wrapping abandon teardown side effects.
  - Result: added `LaunchMessagesDiscardedForAbandon` to the handler action recorder, strengthened the ready-for-abandon teardown smoke test to prove queued launch messages are discarded before abandoned-lobby suppression and postgame `7014` response, and added an arcade launch-failure smoke test proving discard, suppression, then cache-unsubscribed `25` order with pending reset recorded.
  - Stop condition: no production discard behavior changed and no launch coordinator linkage was expanded.

- [x] Rich presence clear recorder coverage pass.
  - Goal: make player postgame cleanup rich-presence clearing visible before adding any lifecycle wrapper around that cleanup path.
  - Result: added `RichPresenceClear` to the handler action recorder and strengthened the player postgame observation smoke test to prove postgame details `26` precedes rich-presence clear, launch peripheral reset, and cache-unsubscribed `25` cleanup.
  - Stop condition: no production rich-presence behavior changed and no launch coordinator linkage was expanded.

- [x] Team-slot publish ordering coverage pass.
  - Goal: cover the 7047 set-team-slot publish/update/ack ordering before adding any helper around lobby mutation publish side effects.
  - Result: added a handler smoke test proving team/slot/bot-difficulty mutation occurs before local member-data publish, shared-lobby publish, details update, and optional `7055` ack.
  - Stop condition: no production team-slot behavior changed and no generic-lobby metadata surface was widened.

- [x] Custom-game loading details-update ordering coverage pass.
  - Goal: make 8052/8053 loading lifecycle details-update order explicit before wrapping launch lifecycle publish/update side effects.
  - Result: strengthened existing started-loading, finished-loading success, and finished-loading failure smoke tests to prove details updates happen after the relevant shared-lobby publish actions and preserve their lifecycle reasons.
  - Stop condition: no production loading lifecycle behavior changed and no launch coordinator linkage was expanded.

- [x] Launch-poll details-update ordering coverage pass.
  - Goal: make the 7034 launch-poll fallback details update order visible before wrapping direct runtime response side effects.
  - Result: strengthened the existing launch-poll smoke test to prove the fallback details update is recorded before the direct `7034` response action and preserves the `7034_launch_poll` reason.
  - Stop condition: no production launch-poll behavior changed and no direct 7034 response seam was introduced.

- [x] Ready-up details-update ordering coverage pass.
  - Goal: make the 7070 ready-up details-update order visible before wrapping custom-game runtime transition side effects.
  - Result: strengthened the existing ready-up smoke test to prove the `7170` response and shared-lobby publish precede the details update, with the `7070_custom_game_ready_up_run_ack` reason preserved.
  - Stop condition: no production ready-up behavior changed and no runtime transition side-effect interface was introduced.

- [x] Broadcast join details-update ordering coverage pass.
  - Goal: make the 7149 broadcast-channel join publish/update/ack order visible before wrapping chat broadcast side effects.
  - Result: added a handler smoke test proving broadcast channel state mutates before shared-lobby publish, details update, and optional `7055` ack, with wrapped/session metadata and reasons preserved.
  - Stop condition: no production broadcast behavior changed and no chat side-effect interface was introduced.

- [x] Broadcast update/close details-update ordering coverage pass.
  - Goal: make the remaining broadcast-channel 7367 and 8054 publish/update order visible before wrapping chat broadcast side effects.
  - Result: added handler smoke tests proving broadcast info update and close mutate local broadcast state before shared-lobby publish and details update, with wrapped/session metadata and reasons preserved.
  - Stop condition: no production broadcast behavior changed and no chat side-effect interface was introduced.

- [x] Leaver-detected details-update ordering coverage pass.
  - Goal: make the 7072 leaver-detected member mutation publish/update order visible before wrapping misc runtime side effects.
  - Result: added a handler smoke test proving member leaver status and connected state mutate before shared-lobby publish and details update, with unwrapped/no-session metadata and reason preserved.
  - Stop condition: no production leaver-detected behavior changed and no misc side-effect interface was introduced.

- [x] LAN server available publish coverage pass.
  - Goal: make the 4511 LAN server available publish visibility explicit before wrapping launch-marker side effects.
  - Result: added a handler smoke test proving the first matching lobby request marks `launch_4511_seen` before shared-lobby publish and repeated matching requests do not republish.
  - Stop condition: no production 4511 behavior changed and server-id sync remains a handler harness stub.

- [x] Focused launch coordinator harness re-check.
  - Goal: determine whether `GBE_PushDotaLaunchStateToClientPeer(...)` can move from pure planner/action-sequence coverage into a production-linked focused harness.
  - Result: defer. `gbe_dota_lobby_launch_coordinator.cpp` still groups launch-state push with 14 other launch/teardown/response members, and the existing handler smoke stubs already define several of those members. Linking the whole TU would create definition conflicts and pull unrelated response/custom-game teardown behavior into a launch-state-only harness.
  - Stop condition: do not link the full launch coordinator TU into handler smoke tests; first create a narrower production seam for launch-state push payload building and side-effect execution.

- [x] Launch-state captured-lobby plan-input seam.
  - Goal: reduce the remaining hand-written field mapping inside `GBE_PushDotaLaunchStateToClientPeer(...)` without touching side effects.
  - Result: added a pure `gbe::dota_lobby_flow` helper that applies captured lobby fields, last pushed game state, and target local steam id to `LaunchStatePushPlanInput`, with focused flow-test coverage.
  - Stop condition: no payload building, response push, rich presence, cache subscription, or last-game-state side effect changed.

- [x] Launch-state payload build sequence seam.
  - Goal: make the launch-state payload build order explicit before introducing a production executor or focused coordinator harness.
  - Result: added a pure `gbe::dota_lobby_flow` build sequence helper that returns `CacheSubscribed` then `DetailsUpdate` for successful plans and an empty sequence for skipped plans.
  - Stop condition: no production payload building, response push, rich presence, cache subscription, or last-game-state side effect changed.

- [x] Launch-state payload build request seam.
  - Goal: describe the payload build requests as pure data before moving any production payload builder calls.
  - Result: added `LaunchStatePayloadBuildRequest` and `launch_state_payload_build_requests(...)` so focused flow tests now cover build order plus `preserve_server_id` propagation for owner-LAN, non-preserve connect-endpoint behavior, and skipped plans.
  - Stop condition: no production payload building, response push, rich presence, cache subscription, or last-game-state side effect changed.

- [x] Launch-state production payload request consumption seam.
  - Goal: have the production launch-state push path consume the pure payload build request sequence without broadening the launch coordinator harness.
  - Result: added `GBE_BuildDotaLaunchStatePayload(...)` as a narrow member helper that delegates each `LaunchStatePayloadBuildRequest` to the existing authoritative cache-subscribed `24` or details-update `26` builder. `GBE_PushDotaLaunchStateToClientPeer(...)` now builds both payloads from the tested request sequence.
  - Stop condition: no response push, cache subscription, rich presence, last-game-state, failure log reason, or harness linkage changed.

- [x] Shared lobby state clear facade planning pass.
  - Goal: identify whether any remaining direct clear semantics can be named without changing behavior.
  - Result: added `shared-lobby-state-clear-plan.md` and indexed it from `docs/gc/README.md`. The plan records the current raw clear surface, separates publish/update mutation paths from clear semantics, and defines safe wrapper names plus stop conditions. No clear behavior changed.
  - Stop condition: planning only unless a behavior-equivalent helper is trivial and already covered.

- [ ] Production-linked reset test re-check.
  - Goal: revisit only if `GBE_ClearDotaLobbyRuntimeState()` gains branching, extra side effects, or a semantic wrapper changes reset ownership.
  - Stop condition: do not add heavy linkage while the production helper remains behavior-equivalent and trivial.

## Always Run Before Handoff

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

Latest handoff verification:

- `bash tools/run_gc_verification.sh --full` passed with payload helper tests `238/238`, handler smoke tests `54/54`, and audit issues `0`.
- `git diff --check` passed.

For source-list, build-system, or new-file changes, also check the relevant `premake5.lua` source lists and `tools/run_gc_offline_tests.sh` entries.
