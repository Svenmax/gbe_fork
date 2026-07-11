# GBE Fork Maintenance Wiki

## Scope

This wiki is the maintenance entry point for the repository, with detailed coverage of the Dota Game Coordinator subsystem. It separates current production architecture from maintenance contracts and historical refactor records.

The root `README.md` remains the user-facing source for emulator setup and general compilation. This wiki focuses on code ownership, safe changes, verification, and production troubleshooting.

## Recommended Reading

1. `ARCHITECTURE.md` - current production structure and critical runtime flows.
2. `GC_MAINTENANCE.md` - invariants and boundaries that changes must preserve.
3. `DEVELOPER_GUIDE.md` - local workflow and change-specific guidance.
4. `TESTING.md` - test layers, audit coverage, CI, and release gates.
5. `TROUBLESHOOTING.md` - production log collection and fault isolation.
6. `INTERFACES.md` - important internal contracts and extension points.

## Current Documents

| Document | Purpose | Authority |
| --- | --- | --- |
| `ARCHITECTURE.md` | Production architecture, ownership, routing, lifecycle, Store, and reconnect flows | Current code and tests |
| `GC_MAINTENANCE.md` | Mandatory invariants, audit boundaries, and architecture investment gates | Current code, Audit 1-27, and gate evidence |
| `DEVELOPER_GUIDE.md` | Development workflow, file placement, and safe change patterns | Current scripts and build configuration |
| `TESTING.md` | Fast/full tests, replay, properties, TSAN, audits, and CI | Current test scripts and workflows |
| `TROUBLESHOOTING.md` | Production diagnostics and regression capture | Current diagnostic code and operational scripts |
| `INTERFACES.md` | Internal interfaces used by GC maintainers | Current headers and production wiring |

## Maintained GC Reference Documents

The following documents under `docs/gc/` remain active references:

| Document | Subject |
| --- | --- |
| `docs/gc/concurrency-ownership.md` | Thread ownership, lock order, and asynchronous boundaries |
| `docs/gc/architecture-investment-gates.md` | Actor, formal-model, and model-CI decision gates |
| `docs/gc/architecture-investment-inputs.json` | Versioned machine-readable gate evidence |
| `docs/gc/reason-trace-governance.md` | High-risk reason inventory |
| `docs/gc/test-coverage-map.md` | Focused test-to-behavior mapping |
| `docs/gc/verification-and-build.md` | GC verification and build entry points |
| `docs/gc/coordinator-boundaries.md` | Coordinator and handler ownership guidance |

## Historical Records

The following documents describe completed work. Use them for design history and regression archaeology; use the Wiki documents above for current maintenance decisions.

| Document group | Status |
| --- | --- |
| `.monkeycode/specs/gc-refactor-next-phase/` | Completed P0-P16 specification and acceptance record |
| `.monkeycode/docs/GC_ARCH_REFACTOR_TASKLIST.md` | Completed earlier architecture closure checklist |
| `.monkeycode/docs/GC_DEV_MIGRATION_PLAN.md` | Historical migration planning |
| `.monkeycode/docs/GC_DELIVERY_SUMMARY.md` | Historical incremental delivery log with phase-local counts |
| `docs/gc/future-refactor-plan.md` | Historical planning input; current investment gates control new architecture work |
| `docs/gc/follow-up-task-list.md` | Completed follow-up plan |
| `docs/gc/next-agent-task-list.md` | Historical handoff plan |
| `docs/gc/*tasklist.md` | Historical path-specific implementation checklists |

## Current Status

- The GC refactor specification is complete through P16.
- Production ownership is application-scoped through `Steam_Client`.
- Typed post-login dispatch, immutable Store snapshots, lifecycle transitions, generation guards, and reconnect adapters are protected by architecture audits.
- Actor migration, a maintained formal model, and model-consistency CI remain closed investments under the current evidence.
- The final recorded gate passed GCC full verification, Clang ThreadSanitizer, 73 audit regression tests, and 27 production architecture audits.

## Documentation Update Rule

Update this wiki in the same change when any of these boundaries move:

- `Steam_Client` ownership or construction order.
- Typed registry shape or request routing.
- Store ownership, generation semantics, or lock order.
- Lifecycle state/event vocabulary or transition effects.
- Reconnect prepare/execute boundaries.
- Verification scripts, audit groups, or blocking CI jobs.
