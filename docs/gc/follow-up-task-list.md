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

- `bash tools/run_gc_verification.sh --full` passed with payload helper tests `225/225`, handler smoke tests `43/43`, and audit issues `0`.
- `git diff --check` passed.

For source-list, build-system, or new-file changes, also check the relevant `premake5.lua` source lists and `tools/run_gc_offline_tests.sh` entries.
