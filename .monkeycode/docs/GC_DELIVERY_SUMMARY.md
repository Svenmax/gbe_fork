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
