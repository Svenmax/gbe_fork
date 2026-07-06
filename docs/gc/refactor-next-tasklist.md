# GC Refactor Next Tasklist

This tasklist continues the Dota GC maintenance refactor. The purpose is maintainability and update safety, not visual cleanup. Each task must reduce real maintenance risk through clearer semantics, stronger tests, or smaller dependency surfaces.

- [ ] 1. Tighten shared lobby read semantics
  - [x] 1.1 Document `GBE_GetSharedDotaLobbyScalarSnapshot()` as a raw immutable snapshot.
    - Clarify that `lobby_id` and `generic_lobby_id` are not valid-gated and may intentionally preserve stale raw field contents.
    - Clarify when callers should prefer `GBE_GetSharedDotaLobbyIdOrZero()` or `GBE_GetSharedDotaGenericLobbyIdOrZero()`.
  - [x] 1.2 Strengthen payload helper tests for raw snapshot semantics.
    - Cover invalid shared state with nonzero raw ids and assert scalar snapshot preserves raw ids.
    - Assert valid-gated id helpers still return zero when `valid == false`.
  - [x] 1.3 Replace one additional low-risk read-only cluster.
    - Candidate clusters: test-side assertions, pure log/suppression reads, or a local read where raw stale ids are intentionally preserved.
    - Stop if the change needs broad snapshot plumbing or changes publish, clear, or lifecycle mutation behavior.

- [ ] 2. Name one more lifecycle-specific shared clear intent
  - [x] 2.1 Confirm existing postgame cleanup coverage.
    - Reuse host-client preserve, ordinary player cleanup, arcade preserve, and host-over-arcade precedence tests.
  - [ ] 2.2 Add a behavior-equivalent wrapper for one clear intent.
    - Preferred candidate: `GBE_ClearSharedDotaLobbyForPlayerPostgameCleanup()`.
    - The wrapper must call `GBE_ClearSharedDotaLobbyState()` without flags, logging, preserve logic, or side effects.
  - [ ] 2.3 Replace exactly one call site.
    - Replace only the ordinary player postgame cleanup shared clear call site if tests already prove behavior.
    - Stop if the path also requires settings, rich presence, server/client ownership, or generic lobby cleanup changes.
  - [x] 2.4 Stop before adding a wrapper without a direct shared clear call site.
    - Ordinary player postgame cleanup currently clears shared state through `GBE_ClearDotaLobbyRuntimeState()` together with local lobby and launch-state reset.
    - Do not split this runtime reset path only to force a lifecycle-specific wrapper.

- [ ] 3. Extend side-effect recorder coverage before more side-effect seams
  - [x] 3.1 Pick one observable side-effect family.
    - Preferred order: server-GC forward, network broadcast, lobby snapshot refresh, rich presence update/clear.
    - Result: selected the existing equip-items full-forward path because it already observes server-GC forward, network broadcast, and lobby snapshot refresh in one tested ordering boundary.
  - [x] 3.2 Add recorder action fields only for the selected family.
    - Preserve target coordinator, target steam id, reason string, wrapped/session/source-job metadata when applicable.
    - Result: strengthened existing recorder metadata for cache-forward steam id, equipped-items network-broadcast source id, and snapshot-refresh reason; reused existing cache-forward target/reason/source-item fields.
  - [x] 3.3 Strengthen one handler smoke test for ordering.
    - Prove mutation-before-publish or response-before-cleanup ordering for the selected path.
    - Stop if heavy SDK fake expansion is required.
    - Result: `test_inventory_equip_full_forward` now asserts local response and save happen before server-GC forward, network broadcast, and lobby snapshot refresh, and verifies the observable steam/source/reason metadata for those side effects.

- [ ] 4. Continue response seam naming only where tests already prove metadata
  - [x] 4.1 Select one emsg family with recorder coverage.
    - Candidate families: practice lobby details update, practice lobby response, cache subscribed.
    - Result: selected the `7014` OtherLeftChannel response family because chat leave tests already prove wrapped/session/reason metadata and response-before-cleanup ordering.
  - [x] 4.2 Add one thin response helper.
    - The helper must delegate to `GBE_PushDotaResponse(...)` and preserve immediate-vs-delayed, wrapped/direct, source-job, apply-lobby-state, and reason semantics.
    - Result: added `GBE_PushDotaOtherLeftChannelResponse(...)` as a thin helper over `GBE_PushDotaResponse(GBE_kDotaOtherLeftChannel, ...)`.
  - [x] 4.3 Replace one or two call sites in the same handler family.
    - Update docs with exact call sites and protected metadata.
    - Result: replaced the stale and normal `7272` chat leave `7014` response call sites in `GBE_HandleDotaLeaveChatChannelRequest(...)`.

- [ ] 5. Add client/ownership seam only after coverage is clear
  - [x] 5.1 Identify one client coordinator lookup path.
    - Candidate: launch-state push to client peer.
    - Result: identified `GBE_PushDotaLaunchStateToClientPeer(...)`, which targets `steam_game_coordinator` when called from the server coordinator and then pushes cache-subscribed/details-update launch state through the client coordinator.
  - [ ] 5.2 Add or strengthen a test for target selection.
    - Cover server coordinator, client coordinator presence, and skip behavior when target is missing or server-owned.
    - Stop: the current handler smoke harness does not link `gbe_dota_lobby_launch_coordinator.cpp`. A direct smoke test would broaden the wrapper linkage surface before the launch coordinator has a focused harness.
  - [ ] 5.3 Add a read-only lookup helper if the test makes ownership behavior reviewable.
    - Stop if the helper needs broad constructor changes or must know unrelated lifecycle phases.
    - Stop: no lookup helper added until target-selection coverage can be added without broad handler-test linkage expansion.

- [ ] 6. Keep payload fixture coverage growing alongside refactors
  - [x] 6.1 Add focused malformed-input coverage for `GBE_ParseDotaEquipOps`.
    - Result: extended parser coverage for slot overflow and style overflow narrowing failures, complementing existing empty, malformed varint, truncated submessage, missing-slot, and class-overflow cases.
  - [x] 6.2 Add focused style-index and attribute-merge coverage for `GBE_ApplyDotaUnlockStyleBitmask`.
    - Result: added coverage for highest valid style index `31` and short attr-400 `value_bytes` merging, complementing existing create, OR, accumulate, and invalid-index tests.
  - [x] 6.3 Add template identifier patching output-equivalence fixtures only when binary output can be asserted exactly.
    - Result: added an exact-output fixture for `GBE_PatchDotaLobbyTemplateIdentifiers`, asserting the full patched byte string for lobby-id varint and steam-id fixed64 replacements.

- [x] 7. Checkpoint before object extraction
  - [x] 7.1 Run full verification.
    - Required command: `bash tools/run_gc_verification.sh --full`.
    - Result: passed with payload helper tests `238/238`, handler smoke tests `43/43`, and audit issues `0`.
  - [x] 7.2 Run whitespace check.
    - Required command: `git diff --check`.
    - Result: passed.
  - [x] 7.3 Re-evaluate Dota sub-object extraction readiness.
    - Start extraction only if shared-state read/clear/publish boundaries are named, at least three side-effect families have recorder coverage, settings/ownership seams are stable, and a small explicit context can replace broad `Steam_Game_Coordinator *` dependency.
    - Decision: defer Dota sub-object extraction. Shared-state and response seams are stronger, and multiple side-effect families have recorder coverage, while client/ownership target-selection coverage remains deferred and the candidate launch-state path still requires a broad coordinator surface.
