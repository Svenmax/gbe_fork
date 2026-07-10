# Dota GC Delivery Summary

## Current Work Branch

- Workspace: `/workspace/gbe_fork`
- Branch: `trae/agent-inRF11`
- Tracking branch: `origin/trae/agent-inRF11`
- Status: clean except the pre-existing untracked `docs/superpowers/` directory
- Ahead of tracking branch after this documentation update: 12 commits
- Review range: `origin/trae/agent-inRF11..HEAD`
- Latest P5 implementation commit: `d29b7ef1 test(gc): verify lifecycle action ownership`

## Dev-Based Integration Candidate

- Worktree: `/tmp/opencode/gbe-migration-check-20260707/dev-worktree`
- Branch: `260707-refactor-gc-dev-baseline`
- Base: `origin/dev`
- Status: clean
- Review range: `origin/dev..260707-refactor-gc-dev-baseline`
- Commits:
  - `d9e6e4fc refactor(gc): migrate extraction baseline onto dev`
  - `06de534a refactor(gc): migrate lobby lifecycle closure onto dev`

## Deliverables

Generated local deliverables are under `/tmp/opencode/gbe-migration-check-20260707/deliverables`.

- `README.md`
- `dev-migration-pr.md`
- `original-branch-pr.md`
- `dev-migration-stat.txt`
- `dev-migration-commits.txt`
- `original-branch-stat.txt`
- `original-branch-commits.txt`
- `dev-migration-series/`
- `original-branch-series/`

## Verification Completed

Current work branch:

```bash
tools/run_gc_verification.sh
git diff --check origin/trae/agent-inRF11..HEAD
git submodule foreach 'git status --porcelain'
```

Dev-based integration candidate:

```bash
tools/run_gc_verification.sh
git diff --check origin/dev..HEAD
git submodule foreach 'git status --porcelain'
```

Both branches passed the verification gate.

## Recommended Next Commands

Push the original work branch when preserving the full agent history is desired:

```bash
git push origin trae/agent-inRF11
```

Push the dev-based migration candidate when opening a PR to `dev`:

```bash
git push origin 260707-refactor-gc-dev-baseline
```

Create the `dev` PR from `260707-refactor-gc-dev-baseline` using the content in `/tmp/opencode/gbe-migration-check-20260707/deliverables/dev-migration-pr.md`.

## Integration Recommendation

Use `260707-refactor-gc-dev-baseline` for `dev` integration. It is based directly on `origin/dev`, contains the verified two-commit migration chain, and avoids the unrelated-history review problem on `trae/agent-inRF11`.

## Next Refactor Baseline

- P0 behavior baseline: `f9d7bc48 fix(gc): harden reconnect lifecycle behavior`
- Baseline record: `b8c022dc docs(gc): record next refactor baseline`
- Implementation plan: `.monkeycode/specs/gc-refactor-next-phase/tasklist.md`
- Baseline details: `.monkeycode/specs/gc-refactor-next-phase/baseline.md`
- Payload helper assertions: 252 passed
- Handler smoke tests: 68 passed
- Replay fixtures: 7 passed
- GC audit checks: 8 passed with 0 issues

P1 unified direct and wrapped custom-game lifecycle execution behind a shared coordinator executor.

P3 moved serialized reconnect connection state into `gbe_dota_serialized_connection_state.{h,cpp}`, removed its `<string>` dependency from the shared reconnect header, and added standalone header compile checks.

P4 added `gbe_dota_reconnect_context.{h,cpp}` as the canonical reconnect source pipeline. Shared, recent, local, and generic recovery inputs now use one eligibility and endpoint-normalization builder, with explicit source priority and rejection reasons. The next implementation stage is P8, which introduces a testable production reconnect network boundary.

P8 added `gbe_dota_reconnect_network.{h,cpp}` as the shared production/offline reconnect orchestrator and `gbe_dota_reconnect_network_adapter.{h,cpp}` as the Steam boundary for context lookup, `ConnectByIPAddress`, and `GameServerChangeRequested_t` queueing. `PostConnectionStateMsg()` now delegates reconnect decisions and side effects through injected narrow interfaces while each client or gameserver serialized socket instance retains independent reconnect state and generic recovery probe cache.

P8 verification passed with 156/156 reconnect network assertions, 252/252 payload helper assertions, 68/68 handler smoke tests, 7 replay fixtures, and 8 audit groups with 0 issues. The next implementation stage is P5, which transactionally models lobby lifecycle effects.

P5 added `gbe_dota_lifecycle_actions.{h,cpp}` and one coordinator-owned lifecycle executor for custom-game transitions, `7034` runtime/member updates, and teardown action lists. The planner now emits a deterministic state/member/phase/runtime/local/shared/details sequence, while direct and wrapped adapters preserve wire parsing, response construction, session routing, and existing fallback behavior.

P5 properties cover deterministic action fingerprints across 256 effect combinations, dependency ordering for runtime/local/shared/details actions, and empty-effect no-op execution. Executor tests cover conditional member publish, runtime failure fallback, wrapped response routing, cache-unsubscribed routing, and abort/continue push failure policies.

P5 verification passed with 156/156 reconnect network assertions, 252/252 payload helper assertions, 70/70 handler smoke tests, 7 replay fixtures, and 9 audit groups with 0 issues. The lifecycle ownership audit keeps planners pure and prevents migrated match/wrapped handlers from directly calling the six executor-owned lifecycle side-effect APIs. The next implementation stage is P6 explicit lobby generation.

P6.1 added the dependency-free `gbe_dota_lobby_generation.h` domain primitive. Generation zero represents an unallocated process-local lifecycle, and create, join, leave, reset, and recovery boundaries each allocate exactly one strictly newer `uint64` generation. Allocation saturates at `UINT64_MAX` and reports failure while preserving the maximum value, preventing wraparound from reusing stale asynchronous generations.

P6.1 verification passed through the full GC gate with 156/156 reconnect network assertions, 252/252 payload helper assertions, 70/70 handler smoke tests, 7 replay fixtures, and 9 audit groups with 0 issues. P6.2 will carry this generation into local/shared snapshots and reconnect contexts while retaining `lobby_id` as the wire identity.

P6.2 added an independent generation field to local/shared lobby state, shared reconnect and scalar snapshots, all reconnect source kinds, recent/reconnect contexts, and serialized connection state. Local-to-shared publish, shared-to-local restore, shared/recent/local/generic source mapping, context construction, scalar snapshot reads, partial shared restore, and the production reconnect execution boundary now preserve generation without treating `lobby_id` as the lifecycle token.

P6.2 keeps the existing lobby-ID-scoped connection and callback deduplication behavior until P6.4. The serialized state stores both identities, allowing P6.3 stale-task checks and P6.4 generation-scoped deduplication to migrate independently. Full verification passed with 159/159 reconnect network assertions, 257/257 payload helper assertions, 70/70 handler smoke tests, 7 replay fixtures, and 9 audit groups with 0 issues.

P6.3 connected the generation primitive to production lifecycle commits. The coordinator now owns the monotonic counter, advances Create, Join, Leave, Reset, and Recover exactly at their state commit boundaries, preserves the newly allocated generation across clears, and rejects lifecycle creation when the counter is exhausted.

Delayed runtime lobby updates and their shared-state publish now capture `lobby_id` and generation when queued, then reject stale work before state application or incoming delivery. Postgame abandon, normal-signout, and deferred-reset slots carry the same identity and return explicit current, stale, or empty consume results, preventing an old task from finalizing a newer lifecycle.

Reconnect `GameServerChangeRequested_t` callbacks now carry generation through the narrow reconnect queue interface. A type-agnostic callsystem execution guard preserves the predicate through immediate registration and late-registration replay, then checks the current reconnect generation at the actual callback dispatch boundary. P6.4 lobby-ID deduplication semantics remain unchanged.

P6.3 full verification passed with 160/160 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 73/73 handler smoke tests, 7 replay fixtures, and 9 audit groups with 0 issues. The next implementation stage is P6.4 generation-scoped connection and callback deduplication.

P6.4 added the typed `gbe::dota_connection::DedupKey` with generation, server ID, and endpoint. Serialized reconnect direct-connect and engine-callback deduplication now reset on generation changes, while lobby ID changes within one generation only synchronize protocol identity. Server and endpoint changes remain distinct connection opportunities within the same generation.

The lobby-flow `GameServerChangeRequested_t` and `GameRichPresenceJoinRequested_t` callback bundle now uses the same typed generation key. Both callbacks retain a generation execution guard through immediate dispatch and late callback registration, and explicit launch-peripheral resets can re-arm the bundle within the current generation.

P6.4 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 73/73 handler smoke tests, 7 replay fixtures, and 9 audit groups with 0 issues. Production Linux build generation remains unavailable in this environment because the bundled Premake binary requires GLIBC 2.38; existing Linux and Windows production build workflows continue to provide the production build gate. The next implementation stage is P6.5 generation lifecycle tests.

P6.5 added a production-handler lifecycle regression for immediate `Join -> Leave -> Join` reuse of the same wire lobby ID. The test verifies that the three committed boundaries allocate generations 1, 2, and 3, then confirms that a delayed runtime update captured by the first join is rejected without changing the rejoined lobby or entering the incoming queue.

Together with the existing registered and late-registration callback guard tests, stale postgame and delayed-runtime tests, reset-generation retention test, and same-ID reconnect dedup tests, P6.5 covers the lifecycle matrix required by task 7.5. Full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 74/74 handler smoke tests, 7 replay fixtures, and 9 audit groups with 0 issues. The next implementation stage is P6.6 generation properties.

P6.6 added deterministic property coverage for all three generation invariants. P6-A exercises 64 seeded sequences with 32 mixed Create, Join, Leave, Reset, and Recover boundaries per sequence, requiring each successful allocation to advance exactly one strictly newer generation. P6-B queues an old runtime action across 64 same-lobby-ID lifecycle changes and requires the current lobby state, game state, and incoming queue to remain unchanged. P6-C seeds 64 serialized connection states and requires every generation change to clear retry, payload size, posted server, direct-connect, and callback deduplication state while restoring both connection opportunities.

P6.6 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, and 9 audit groups with 0 issues. The remaining P6 task is the stage checkpoint in 7.7.

P6 is complete. The stage checkpoint confirms explicit generation allocation at every lifecycle boundary, generation propagation through reconnect and deferred work, stale execution guards at actual dispatch time, generation-scoped connection deduplication, same-wire-ID lifecycle regression coverage, and deterministic properties P6-A through P6-C. The bundled Linux Premake binary still requires GLIBC 2.38, so Linux and Windows CI workflows remain the production build gate for this environment. The next implementation stage is P7 shared lobby state ownership.

P7.1 added the dependency-free `gbe::dota_lobby_state::Store` contract around `GBE_SharedDotaLobbyState`. The interface returns immutable value snapshots, accepts complete publish values, clears to a zero state, applies mutations to a copied candidate before commit, and rejects stale generation compare/update operations before invoking their mutator. The store receives its state and recursive mutex dependencies explicitly, allowing production to retain the existing synchronization domain while focused tests use isolated fixtures.

The new focused store suite covers empty snapshots, complete publish, snapshot copy isolation, multi-field update commits, matching generation updates, stale generation preservation, and complete clear. The header compiles independently, shell and Premake source lists include the production implementation, and full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the store suite, and 9 audit groups with 0 issues. The next implementation task is P7.2 binding production shared state and `global_mutex` behind this store.

P7.2 bound a single production store instance to the existing `GBE_shared_dota_lobby_state` object and `global_mutex`, preserving the established recursive synchronization domain and avoiding a second lock order. `GBE_ClearSharedDotaLobbyState()` now clears through this store, while the accessor is exposed through a forward-declared internal boundary so unrelated GC translation units do not inherit the store's DTO and mutex dependencies.

The focused store suite now runs four concurrent readers against 2,000 complete version publishes. Each snapshot must carry matching generation, lobby ID, server ID, endpoint, and cache-service version fields, proving readers observe one committed value version. P7.2 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the concurrent store suite, and 9 audit groups with 0 issues. The next implementation task is P7.3 migrating shared lobby read paths to one snapshot per business operation.

P7.3 introduced `GBE_GetSharedDotaLobbyStateSnapshot()` as the complete value-snapshot facade over the production store and migrated shared lobby reads to it. Scalar and reconnect snapshot helpers, arcade detection, client/server restore, direct and wrapped SourceTV/watch/spectate responses, joinable custom modes, normal-signout finalization, owner fallback, and coordinator diagnostics now capture one shared snapshot per business operation and derive every field from that version.

Production direct field access is now limited to the store backing object, diagnostic backing-address logging, and the publish/runtime write paths reserved for P7.4. Offline wrapper seams return their fixture state by value, keeping the same production read contract without duplicating store behavior. P7.3 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the store suite, and 9 audit groups with 0 issues. The next implementation task is P7.4 migrating shared lobby writes through generation-aware store operations.

P7.4 migrated production shared lobby writes behind the store. Complete local-to-shared publish preserves the existing client/server merge rules while committing through a monotonic generation operation that accepts the current or a newer generation and rejects an older lifecycle. Generic-lobby metadata connect/server updates, 4508 runtime connect adoption, and gameserver-derived server ID updates now use generation-aware compare/update and emit explicit stale diagnostics when their captured local generation no longer matches the store.

Clear remains centralized through the store, and production direct field writes to `GBE_shared_dota_lobby_state` are now eliminated. Focused tests cover current/newer complete publish, stale complete publish, matching compare/update, stale compare/update, and a delayed writer attempting to overwrite a replacement generation. P7.4 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the store suite, and 9 audit groups with 0 issues. The next implementation task is P7.5 removing the mutable production extern surface and adding an audit against direct shared-state access.

P7.5 removed the mutable shared lobby extern from the production internal header. The backing `GBE_SharedDotaLobbyState` is now a function-local static owned by `GBE_GetSharedDotaLobbyStateStore()`, so production translation units can only obtain value snapshots or invoke store operations. Existing pointer diagnostics retain a stable identity by logging the store instance address.

Audit 10 scans every Dota GC production translation unit plus `gbe_dota_gc_internal.h` after comment stripping and fails on any use of the retired `GBE_shared_dota_lobby_state` symbol. Focused offline fixtures keep independent mutable DTOs for test setup. P7.5 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the store suite, and 10 audit groups with 0 issues. The next implementation task is P7.6 completing the named store unit and concurrency coverage matrix.

P7.6 completed the named store unit and concurrency coverage matrix. Existing tests cover immutable snapshot copies, complete publish and clear, matching and stale generation compare/update, monotonic complete publish, and delayed stale writers. A new five-writer interleaving commits 2,000 matching-generation scalar and vector updates while rejecting 500 stale-generation mutations, proving matching updates are serialized without lost writes and stale mutators cannot affect the endpoint or member list.

A second interleaving runs four readers across 1,000 alternating complete publishes and clears. Every observed snapshot must be either the complete zero state or one complete published version, and the final publish must remain intact. The focused store binary passed five consecutive runs, and P7.6 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 257/257 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the store suite, and 10 audit groups with 0 issues. The next implementation task is P7.7 adding deterministic store properties P7-A through P7-C.

P7.7 added deterministic coverage for all three shared-store properties. P7-A publishes 32 complete scalar, string, and vector versions for each of 64 seeds and requires every returned snapshot to match exactly one published version. P7-B attempts one older-generation compare/update across 64 distinct current states and requires the mutator to remain uncalled while every tracked field stays unchanged.

P7-C runs the real payload-helper ID facades over the production `Store` implementation in the offline wrapper. For 64 nonzero lobby and generic-lobby ID pairs, the test clears through `GBE_ClearSharedDotaLobbyState()` and requires both valid-gated ID helpers to return zero. Payload-helper assertions increased from 257 to 449. P7.7 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 449/449 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the store property suite, and 10 audit groups with 0 issues. The remaining P7 task is the stage checkpoint in 8.8.

P7 is complete. The stage checkpoint confirms one production shared-lobby store in the existing `global_mutex` synchronization domain, immutable value snapshots for all readers, generation-aware writes for asynchronous producers, complete clear semantics, removal and audit protection of the mutable production global surface, concurrent linearization coverage, and deterministic properties P7-A through P7-C. The bundled Linux Premake binary still requires GLIBC 2.38, so Linux and Windows CI workflows remain the production build gate for this environment. The next implementation stage is P9 typed GC handler registry ownership.

P9.1 defined the dependency-light typed registry contract in `gbe_dota_handler_registry.h`. Each aggregate entry carries a message ID, a strong direct/wrapped mode mask, a wrapped-session policy, a lifecycle classification, a uniform adapter function pointer, and a stable handler identity. Constexpr helpers centralize request-path matching and wrapped-session forwarding semantics without changing the active dispatcher table.

An independent header compile test proves the contract is self-contained and validates aggregate initialization plus direct, wrapped, unknown-path, session, and handler identity semantics at compile time. Shell and Premake test source lists include the new contract. P9.1 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 449/449 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, the registry header test, and 10 audit groups with 0 issues. The next implementation task is P9.2 migrating the active post-login dispatch mapping to typed registry entries.

P9.2 migrated all 24 active post-login mappings to typed registry entries. The 16 shared lobby and chat handlers declare `DirectAndWrapped` mode with wrapped-session forwarding, while the 8 standalone misc handlers declare `Direct` mode with session ignored. Lookup now requires both message ID and request-path support, and the selected entry's session policy controls whether the adapter receives the outer session field.

Adapter bodies, table scan order, successful dispatch logging, and unknown or mode-mismatched fallback remain unchanged. Audit 4 now parses typed entries while continuing to verify every adapter-to-handler call and every direct-only path guard. P9.2 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 449/449 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, all 24 typed dispatch mappings, and 10 audit groups with 0 issues. The next implementation task is P9.3 deriving audit data from the registry and removing the parallel manual dispatch inventory.

P9.3 made the production typed registry the sole dispatch inventory. Audit 4 parses all six entry fields directly from `kTable`, then resolves each referenced adapter lambda and extracts its actual `GBE_HandleDota*Request` call. The audit derives its entry count from the registry and rejects partially parsed entries, missing or orphaned adapters, adapters that call multiple or zero request handlers, missing direct-only path guards, direct entries that forward wrapped session metadata, unknown handler identities, and unknown lifecycle classes.

The two manually maintained Python dispatch lists were removed, eliminating the parallel source of truth. P9.3 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 449/449 payload helper assertions, 75/75 handler smoke tests, 7 replay fixtures, 24 dynamically audited registry entries, and 10 audit groups with 0 issues. The next implementation task is P9.4 associating high-risk registry entries with smoke or replay fixture identifiers.

P9.4 added a dependency-light fixture identifier to each typed registry entry. All 13 `LobbyMutation` and `LobbyLifecycle` entries now reference a live `smoke:<test_name>` or `replay:<fixture>:<label>` identifier; lower-risk read and misc entries retain an empty fixture field. Direct handler smoke coverage was added for wrapped 4512 invitation-created responses and declined 4513 remove-invite/cache-unsubscribe ordering, closing the two high-risk coverage gaps found during the registry inventory.

Audit 4 now loads the actual handler smoke functions and replay fixture labels, then rejects high-risk entries with no fixture, malformed identifiers, missing smoke tests, missing replay files, or stale replay labels. P9.4 full verification passed with 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 449/449 payload helper assertions, 77/77 handler smoke tests, 7 replay fixtures, all 24 typed registry entries, all 13 high-risk fixture associations, and 10 audit groups with 0 issues. The next implementation task is P9.5 adding focused registry unit tests.

P9.5 centralized typed registry lookup in the dependency-light contract. The constexpr `find_entry()` helper now owns message-ID and request-path selection, and the production post-login dispatcher delegates its historical linear lookup to that helper. `has_unique_message_ids_per_mode()` expresses the registry uniqueness invariant while allowing one message ID to have disjoint direct and wrapped entries.

The new focused registry suite uses direct-only, wrapped-only, dual-mode, disjoint same-ID, and overlapping duplicate tables to cover per-mode uniqueness, all request-path combinations, wrapped-session policy, unknown message/path fallback, and stable handler identity selection. Shell and Premake test wiring include the standalone binary. P9.5 full verification passed with 19/19 registry assertions, 165/165 reconnect network assertions, 4/4 callsystem guard assertions, 449/449 payload helper assertions, 77/77 handler smoke tests, 7 replay fixtures, and 10 audit groups with 0 issues. The next implementation task is P9.6 adding deterministic registry properties P9-A through P9-C.
