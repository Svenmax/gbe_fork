# GC Developer Guide

## Prerequisites

GC focused development requires:

- Bash.
- Python 3.
- A C++17 compiler.
- Git with the repository Submodules initialized.
- Clang for ThreadSanitizer.

The root `README.md` contains full Windows and Linux dependency setup for production builds.

## Initial Setup

Initialize Submodules before building:

```bash
git submodule update --init --recursive --depth 1
```

Confirm the working compiler:

```bash
c++ --version
python3 --version
```

## Standard Workflow

Use the fast gate while iterating:

```bash
bash tools/run_gc_verification.sh --fast
```

Use the full gate before handing off a completed GC change:

```bash
CXX=c++ bash tools/run_gc_verification.sh --full --base-sha origin/dev
```

Run TSAN for changes involving shared state, callbacks, reconnect, generation, queues, locks, or object lifetime:

```bash
CXX=clang++ bash tools/run_gc_tsan_tests.sh
```

Use a Clang full pass when changing headers, template-heavy code, or compiler-sensitive control flow:

```bash
CXX=clang++ bash tools/run_gc_verification.sh --full --base-sha origin/dev
```

## Change Workflow

For a production bug:

1. Capture the message ID, request path, lobby ID, generation, server ID, and operation sequence.
2. Reproduce the failure in the smallest suitable test layer.
3. Add a failing regression test.
4. Make the smallest change in the owning layer.
5. Run the focused test and fast verification.
6. Run full verification before delivery.
7. Add TSAN when the change crosses an asynchronous or synchronization boundary.

## Owning Layer Selection

| Change | Preferred owner |
| --- | --- |
| Wire parsing or wrapping | Request router or wire helper |
| Message-to-handler mapping | Canonical typed registry |
| Request-specific orchestration | Domain handler |
| State decision or action ordering | Pure planner or lifecycle transition |
| Shared lobby mutation | Store or Store-facing coordinator operation |
| Multiple observable side effects | Typed action list and lifecycle executor |
| Steam network call | Reconnect or network adapter |
| Callback delivery | Callback queue and execution guard |
| Delayed lifecycle work | Coordinator queue with generation capture |
| Diagnostic classification | Typed diagnostic event model |

## Adding A Post-Login Message

1. Determine direct, wrapped, or dual mode.
2. Define wrapped-session behavior.
3. Add a typed registry entry to the canonical production table.
4. Reuse an existing adapter or add a narrow adapter.
5. Keep payload parsing and response construction in the appropriate domain handler/helper.
6. Add a high-risk fixture when the entry mutates lobby or lifecycle state.
7. Run `gbe_dota_handler_registry_test`, the handler suite, replay where applicable, and Audit 4.

Avoid a parallel message switch or a second table. Audit 4 and Audit 14 treat the typed registry as the canonical mapping source.

## Adding A Lifecycle Transition

1. Extend the typed state/event vocabulary.
2. Add or update the pure transition function.
3. Classify every affected state/event pair.
4. Return typed effects without performing side effects.
5. Map effects to existing planner/action/executor behavior.
6. Add example tests for accepted, ignored, duplicate, and rejected cases.
7. Extend properties and the differential reference model.
8. Add a production-path fake or handler regression.
9. Refresh `docs/gc/architecture-investment-inputs.json` for a major state-space expansion.

## Adding Asynchronous Work

Every asynchronous operation that can observe or mutate lobby state must:

1. Capture lobby ID and generation at queue time when state identity requires both.
2. Copy payload and guard metadata into the queue entry.
3. Validate generation at the final execution boundary.
4. Reject stale work before state mutation or callback delivery.
5. Preserve queue ordering for protocol-visible work.
6. Add a leave/rejoin or replacement-generation regression.

## Shared Store Changes

- Capture one complete immutable snapshot per business operation.
- Use generation-aware publish or compare/update for lifecycle-sensitive writes.
- Keep Store mutators free from external side effects.
- Publish reconnect context only after the shared-state commit succeeds.
- Preserve client participant checks during shared-state adoption.
- Add focused Store tests and bounded concurrency coverage.

## Reconnect Changes

- Keep source mapping in `gbe_dota_reconnect_context.*`.
- Keep decision/state mutation in reconnect prepare.
- Keep `ConnectByIPAddress` and callback queueing in the adapter/effect phase.
- Preserve the lock order documented in `docs/gc/concurrency-ownership.md`.
- Keep the dedup key composed of generation, server ID, and endpoint.
- Add focused connector/callback fakes and generation regressions.

## New Source Files

Production files under `dll/` normally enter production Premake targets through `common_files`. Offline tests use explicit source lists.

When adding a testable production `.cpp` file:

1. Add it to each affected command in `tools/run_gc_offline_tests.sh`.
2. Add it to relevant Premake test targets in `premake5.lua`.
3. Keep test-only wrappers and stubs out of production targets.
4. Add headers to Premake target lists for project visibility where appropriate.
5. Run Audit 6 to verify source-list inclusion.

## Production Builds

Production PR gates build `api_experimental` x64 release on Windows and Linux. The repository-provided Premake binaries and Submodules are the authoritative CI environment.

The bundled Linux Premake currently requires GLIBC 2.38. Environments with older glibc can still run focused offline, audit, replay, and TSAN gates; production Linux integration remains the blocking CI job.

## Documentation Discipline

- Update `ARCHITECTURE.md` when ownership or runtime flow changes.
- Update `GC_MAINTENANCE.md` when an invariant or audit boundary changes.
- Update `TESTING.md` when scripts, targets, counts, or CI jobs change.
- Update `TROUBLESHOOTING.md` when diagnostic fields or operational paths change.
- Keep completed task lists as historical records.
