# Dota GC Maintenance Contract

## Maintenance Goal

The completed refactor established explicit ownership and verification boundaries. Future maintenance should preserve those boundaries while fixing production defects with small, evidence-driven changes.

## Core Invariants

### Ownership

- `Steam_Client` is the production application owner.
- Client and gameserver coordinators share one Store.
- Each role owns independent reconnect adapter, serialized state, callback path, and lifecycle executor.
- Coordinators are destroyed before their dependencies.
- Compatibility locators remain non-owning.
- Mutable GC business state belongs to application-owned instances.

### Routing

- The typed post-login registry is the canonical mapping source.
- Direct and wrapped requests use one normalized context.
- Wrapped sessions follow explicit entry policy.
- High-risk entries retain live test fixtures.
- Special routes remain explicit until a safe typed context exists.

### Handler And Effects

- Handlers parse, map, orchestrate, adapt responses, and log.
- Planners and transitions remain pure.
- Executors own observable side effects.
- Multi-effect paths preserve typed action order.
- Historical compatibility operations may shrink and must not expand casually.

### Shared State

- Production shared lobby access goes through Store.
- Business logic captures one complete immutable snapshot per operation.
- Store mutators perform local deterministic value changes only.
- Stale generation writes do not commit.
- Client adoption requires participant ownership or membership.
- Shared publication succeeds before reconnect context is cached.

### Lifecycle And Generation

- Core lifecycle decisions use typed pure transitions.
- Every state/event pair remains classified.
- Create, join, leave, reset, and recover advance generation.
- Generation never wraps.
- Asynchronous work captures generation at creation.
- Final execution validates generation.
- State-applying delayed work also validates lobby ID.
- Retrieval-driven teardown boundaries remain protocol-visible.

### Reconnect And Concurrency

- Context mapping has one canonical owner.
- Reconnect preparation is separate from external execution.
- Dedup identity includes generation, server ID, and endpoint.
- External network and callback effects execute outside process and instance locks.
- Callback delivery validates current generation.
- Lock order remains process mutex followed by one serialized-instance mutex.

## Audit Protection

The production audit suite contains 27 groups. Important enforcement areas include:

- Canonical registry and fixture mapping.
- Handler side-effect baseline.
- Typed diagnostic reason inventory.
- Lifecycle executor ownership.
- Store-only shared state access.
- Concurrency and lock-order contract.
- Retired reconnect and shared-lobby layers.
- Composition-root construction and destruction.
- Mutable global-state allowlist.
- Lifecycle transition gates.
- Handler responsibility and state/effect ownership.
- Generation-scoped asynchronous work.
- High-risk test credibility.
- Independent blocking CI jobs.
- Architecture investment decision consistency.

When a new structural boundary is introduced, extend the audit with both accepted and rejected fixtures.

## Historical Compatibility Seams

Some handlers retain direct response pushes and local-lobby commits to preserve stable protocol order. Audit 20 records the accepted operation baseline and allows it to remain stable or shrink.

For new behavior:

- Put state decisions in a planner or transition.
- Put shared state in Store-facing operations.
- Put multi-step effects in the executor.
- Keep wire adaptation at the handler/router edge.
- Avoid adding a new direct compatibility operation when an existing owner can express it.

## Architecture Investment Gates

### Actor Or Single-Owner Migration

Re-evaluate when any trigger appears:

- A reproducible TSAN race remains after applying the current ownership contract.
- A production operation requires lock depth beyond the documented two-lock order or requires reversed/cyclic acquisition.
- Two confirmed defects in one delivery phase arise from out-of-order callbacks mutating the same lifecycle state after generation validation.
- A direct multi-thread business-state writer appears outside the approved Store and queue owners.

Current decision: closed.

### Maintained Formal Model

Re-evaluate when the critical-failure trigger appears, or when at least two scale/defect/maintenance triggers appear:

- State space exceeds 16 states, 24 events, or 384 reachable pairs.
- Two duplicate, stale, or ordering defects escape existing example, property, differential, and production-path tests in one phase.
- Lifecycle failure can irreversibly corrupt persisted user data, isolation, or remote protocol state.
- Three or more active maintainers modify the transition contract in one release.

Current decision: closed.

### Model Consistency CI

This gate requires an open formal-model gate, a maintained model owner and reviewer, a versioned machine-readable vector schema, a pinned checker, and a deterministic CI command under ten minutes.

Current decision: closed.

Versioned evidence lives in `docs/gc/architecture-investment-inputs.json`. Audit 19 and Audit 27 validate the inputs and conclusions.

## Safe Stop Condition

The architecture phase is complete while all conditions remain true:

- Production defects can be reproduced in existing test layers.
- New fixes fit existing owners without widening compatibility seams.
- TSAN remains clear.
- Lock depth remains two.
- Lifecycle state space remains within the current verification strategy.
- Architecture audits and production builds remain blocking.

Under these conditions, production testing and regression capture provide more value than preventive architecture expansion.

The GC maintenance hardening cycle completed with GCC full verification, Clang full verification, Clang TSAN, architecture audits, production dispatcher integration coverage, behavior replay, locator lifecycle coverage, and generation-aware Store cleanup passing. Work now follows demand-driven maintenance. Composition Root production takeover, a single authoritative lifecycle state machine, and deeper coordinator decomposition require concrete feature pressure or reproducible maintenance friction.

## Required Verification By Change Type

| Change type | Required gate |
| --- | --- |
| Parser, payload, or narrow handler fix | Focused test and fast verification |
| Registry or lifecycle mutation | Full verification |
| Store, callback, reconnect, generation, queue, or lock change | Full verification and Clang TSAN |
| Source-list or build configuration change | Full verification and affected production build |
| Architecture boundary change | Audit positive/negative fixtures, full verification, TSAN when concurrent, and Wiki update |
| Major concurrency or lifecycle expansion | Refresh investment evidence and rerun Audit 19/27 |
