# Dota GC Follow-up Task List

This is the executable follow-up checklist for Dota GC maintenance work after the coordinator refactor. It should stay practical: every task needs a clear behavior goal, a test/verification expectation, and a stop condition.

Reference docs:

- `future-refactor-plan.md`: overall sequence and stop conditions.
- `shared-lobby-state-contract.md`: `GBE_shared_dota_lobby_state` contract and facade rules.
- `lifecycle-state-map.md`: lifecycle paths, risks, and state transitions.
- `test-coverage-map.md`: which offline test to extend for each kind of change.

## P0. Before More Refactor

Do these before broad shared-state or side-effect refactors.

- [ ] Add a focused test proving a host client observing postgame does not clear shared state still needed by the server GC.
  - Goal: protect the server/client ownership boundary before lifecycle cleanup helpers grow.
  - Suggested place: `tools/gbe_dota_handler_test/smoke_test.cpp` or a focused lifecycle test if production linkage becomes practical.
  - Verify: `bash tools/run_gc_verification.sh --full`.

- [ ] Add a focused test for reconnect preserve-vs-clear behavior.
  - Goal: make it explicit which lifecycle paths preserve reconnect context and which paths clear it.
  - Suggested place: `tools/gbe_dota_gc_payload_helpers_test/gbe_dota_gc_payload_helpers_test.cpp` for payload decisions; handler/lifecycle test for cleanup triggers.
  - Verify: `bash tools/run_gc_verification.sh --full`.

- [ ] Add or strengthen a stale postgame chat leave test.
  - Goal: prove a cleared shared state is not republished from stale local lobby state.
  - Suggested place: adjacent to `test_chat_leave_postgame_channel_order`.
  - Verify: `bash tools/run_gc_verification.sh --full`.

- [ ] Decide whether the reset contract needs a production-linked focused test.
  - Current state: `test_lobby_runtime_reset_clears_local_shared_and_last_launch_state` protects the handler-test stub contract, while the production helper is small and behavior-equivalent.
  - Goal: avoid overbuilding unless future reset helpers become less trivial.
  - Stop condition: do not add heavy linkage just to satisfy aesthetics.

## P1. Shared Lobby State Read-Only Facade

Start with read-only access. Do not change publish or clear behavior in this phase.

- [ ] Add one or more read-only helpers.
  - Candidate helpers: `GBE_HasSharedDotaLobbyState()`, `GBE_GetSharedDotaLobbyStateSnapshot()`, `GBE_GetSharedDotaLobbyIdOrZero()`, `GBE_IsSharedDotaLobbyActive()`.
  - Rule: do not return a mutable reference.

- [ ] Add focused tests for helper equivalence.
  - Goal: direct-field reads and helper reads produce the same decisions.
  - Suggested place: payload helper tests for reconnect/read decisions; handler smoke tests only when side effects matter.

- [ ] Replace one small cluster of obviously read-only call sites.
  - Good first target: payload helper or decision code that only reads validity/lobby id/active state.
  - Bad first target: publish, clear, postgame cleanup, or mixed mutation paths.

- [ ] Run verification.
  - Required: `bash tools/run_gc_verification.sh --full`.
  - Required: `git diff --check`.

- [ ] Stop if review gets harder.
  - If call chains become longer without clearer lifecycle intent, revert or shrink the step.

## P1. Lifecycle Helpers And Full Reset Semantics

Only add helpers that name real lifecycle intent. Keep message order stable.

- [ ] Keep `GBE_ClearDotaLobbyRuntimeState()` as the full local/shared/last-launch reset helper.
  - Goal: preserve the existing full-reset meaning.

- [ ] Consider a normal-signout semantic helper only after tests are in place.
  - Candidate name: `GBE_ClearDotaRuntimeForNormalSignout(...)`.
  - Must preserve cache-unsubscribe ordering.

- [ ] Consider a postgame cleanup semantic helper only after tests are in place.
  - Candidate name: `GBE_FinalizeDotaPostgameCleanup(...)`.
  - Must preserve host-client/server-GC ownership behavior.

- [ ] Keep partial cleanup exceptions explicit.
  - Example: stale postgame chat leave can clear local state without broad runtime reset.
  - Rule: do not fold exceptions into generic helpers unless tests prove the behavior remains identical.

## P1. Side-Effect Recorder Coverage

Before wrapping more side effects, make ordering assertions stronger.

- [ ] Extend handler recorder coverage for one high-risk path at a time.
  - Candidate paths: normal signout, postgame cleanup, lobby destroy, stale chat leave, set-details publish/update.

- [ ] Assert ordering, not just presence.
  - Examples: cache unsubscribe before cleanup, mutation before publish, 7014 before postgame local cleanup, details update after publish.

- [ ] Document any reason-string changes.
  - If a high-risk reason string changes or is added, update `reason-trace-governance.md` and keep `_audit_gc_refactor.py` green.

## P2. Shared Lobby State Clear Facade

Only start after read-only facade work is stable.

- [ ] Add behavior-equivalent clear helper.
  - First implementation should be exactly equivalent to assigning `GBE_SharedDotaLobbyState{}`.
  - Do not add preserve logic in the first clear-helper PR.

- [ ] Replace one direct clear site.
  - Choose the least ambiguous full-clear path first.
  - Avoid postgame/server-client ownership paths until tests are stronger.

- [ ] Verify no stale re-publish behavior changed.
  - Run full verification.
  - Add focused test if existing coverage is indirect.

- [ ] Only later split clear helpers by lifecycle intent.
  - Candidate intents: normal signout, player postgame cleanup, full runtime reset.

## P2. Dependency Seams

Centralize dependencies only when doing so improves review or testability.

- [ ] Add read-only wrappers for settings/lobby lookups where repeated and risky.
  - Candidate: host/server lobby ownership checks.

- [ ] Add named helper for settings lobby clear only if ordering remains obvious.
  - Candidate: `GBE_ClearSettingsLobbyForDotaSignout(...)`.

- [ ] Wrap rich-presence operations only after recorder/test coverage can assert ordering.

- [ ] Keep wrappers behavior-equivalent first.
  - No retry policy, logging policy, or preserve semantics in the first seam step unless already tested.

## P2/P3. Inventory, VPK, And Template Replay

Lower priority unless these areas become active bug sources.

- [ ] Keep `GBE_vpk_loot_data` mutation centralized through `GBE_SetDotaVpkLootData(...)`.

- [ ] Add tests before changing inventory/VPK ownership.
  - Suggested tests: handler inventory smoke tests, custom-game tests, replay fixtures when output-visible.

- [ ] Do not move template blobs for appearance alone.
  - Fixture output must stay stable if blob ownership changes.

## P3. Dota Sub-Object Extraction Decision

Do not start until shared-state facade, lifecycle tests, and dependency seams are stable.

- [ ] Re-evaluate whether a Dota sub-object can avoid depending on the entire `Steam_Game_Coordinator *`.

- [ ] Extract only if the new object has a small explicit context and can be tested directly.

- [ ] Stop if extraction creates several smaller objects that still share the same implicit global state.

## Always Run Before Handoff

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

For source-list, build-system, or new-file changes, also check the relevant `premake5.lua` source lists and `tools/run_gc_offline_tests.sh` entries.