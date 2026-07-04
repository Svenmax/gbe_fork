# GC Refactor Phase 3 Plan

## Objective

Phase 3 turns the current mechanical file split into a lower-risk architectural cleanup. The goal is to reduce the new large files, tighten dependency boundaries, and improve regression protection before introducing heavier abstractions.

The current Phase 2 result improved file organization, but it still leaves two large modules and broad cross-translation-unit coupling:

- `dll/gbe_dota_handlers.cpp` remains a large handler bucket.
- `dll/gbe_dota_gc_payload_helpers.cpp` remains a large payload helper bucket.
- Split translation units share near-identical include blocks.
- Internal declarations and implementation locations require audit cleanup.

Phase 3 should stay incremental. Each step must preserve behavior and keep the offline GC tests passing.

## Guiding Principles

- Prefer small, reversible refactors over large rewrites.
- Preserve existing function signatures until tests cover the affected behavior.
- Split by observable responsibility first, introduce abstractions only after repeated patterns are proven.
- Improve tests before changing logic-heavy code.
- Keep `Steam_Game_Coordinator` as the public facade until a smaller extraction is justified.

## Non-Goals

- Do not introduce a full Facade/Application/Domain/Infrastructure layering in one pass.
- Do not introduce Command, Builder, State, Observer, and Factory patterns at the same time.
- Do not rewrite Dota GC protocol logic while splitting files.
- Do not change wire output formats except when a bug fix is explicitly covered by tests.

## Phase 3.0: Stabilize The Current Refactor

### Scope

Fix low-risk issues that make future refactoring harder to trust.

### Work

- Fix `tools/_audit_gc_refactor.py` so it works from the repository root without hard-coded `/workspace` paths.
- Review all audit findings and classify them as real issues or script false positives.
- Remove real zombie declarations from `dll/gbe_dota_gc_internal.h`.
- Add declarations for real shared functions or mark private helper functions as `static` / anonymous namespace where appropriate.
- Update stale line references in `REFACTOR_TODO.md` or replace line references with function-name anchors.
- Add the audit script to the local verification flow after it is reliable.

### Acceptance Criteria

- `tools/_audit_gc_refactor.py` runs from `gbe_fork/` without path edits.
- The audit reports zero real zombie declarations.
- The audit reports zero real under-exposed shared definitions.
- `tools/run_gc_offline_tests.sh` passes.

## Phase 3.1: Split Handler Buckets By Domain

### Scope

Reduce `gbe_dota_handlers.cpp` from a second large file into smaller domain-specific handler files.

### Proposed Files

- `dll/gbe_dota_lobby_handlers.cpp`
- `dll/gbe_dota_chat_handlers.cpp`
- `dll/gbe_dota_inventory_handlers.cpp`
- `dll/gbe_dota_match_handlers.cpp`
- `dll/gbe_dota_misc_handlers.cpp`
- `dll/gbe_dota_handlers.cpp` retained only for shared handler-local helpers or removed if empty.

### Work

- Group existing `GBE_HandleDota*` functions by responsibility.
- Move one group at a time.
- Keep signatures unchanged.
- Keep behavior unchanged.
- Move handler-only static constants with the functions that use them.
- Keep cross-domain shared helpers in a small internal helper file only when multiple handler groups need them.
- Reduce repeated include blocks during each move.

### Acceptance Criteria

- `dll/gbe_dota_handlers.cpp` is under 1500 lines, or under 2000 lines with a documented reason why further splitting would harm cohesion (e.g. remaining handlers share post-login protocol context that cannot be split without a dispatch-table refactor). The original 1000-line target proved unrealistic because the post-login/socket/template-replay handlers share protocol-level state that resists mechanical splitting before Phase 3.3.
- No domain handler file exceeds 1500 lines without a follow-up split plan. If a domain file (e.g. lobby) exceeds 1200 lines after logic refactoring, split it by sub-domain (e.g. invite / lifecycle / set-details) before marking the phase complete.
- No new public API is introduced just for file movement.
- Each new handler file has a clear domain responsibility.
- Offline GC tests pass after each group move.
- A handler-level test harness exists before Phase 3.1.7 logic refactoring begins (see tasklist 3.1.6.5).

## Phase 3.2: Separate Pure Payload Helpers From Stateful Helpers

### Scope

Reduce `gbe_dota_gc_payload_helpers.cpp` by separating pure byte/protobuf utilities from helpers that depend on coordinator state or global state.

### Proposed Files

- `dll/gbe_dota_payload_wire_helpers.cpp`
- `dll/gbe_dota_payload_item_helpers.cpp`
- `dll/gbe_dota_payload_lobby_helpers.cpp`
- `dll/gbe_dota_gc_payload_helpers.cpp` retained for orchestration-level payload composition.

### Work

- Identify pure functions that only transform inputs into outputs.
- Move pure wire parsing/building helpers first.
- Move item serialization helpers next.
- Move lobby payload composition helpers last.
- Add focused tests for pure helpers before moving logic-heavy code.
- Avoid a builder abstraction until the same construction pattern appears in at least three places.

### Acceptance Criteria

- `dll/gbe_dota_gc_payload_helpers.cpp` is under 1200 lines.
- Pure helper files have no dependency on `Steam_Game_Coordinator`.
- Item serialization tests verify owner, attributes, equip state, custom name, and custom description behavior.
- Wire helper tests cover malformed varint, truncated fields, unknown wire types, and valid round trips.
- Offline GC tests pass.

## Phase 3.3: Introduce A Lightweight Handler Dispatch Table

### Scope

Shorten top-level Dota request dispatch without introducing a class hierarchy.

### Design

Use a static table of message IDs and member function pointers before considering polymorphic handler objects.

```cpp
struct DotaHandlerEntry {
    uint32 emsg;
    bool (Steam_Game_Coordinator::*handler)(const gbe::dota_gc_router::DotaGcRequestContext &context);
};
```

### Work

- Normalize handler signatures where the change is mechanical and covered by tests.
- Create the dispatch table in a private implementation file.
- Replace repetitive switch branches with a lookup and call.
- Preserve special cases only where the handler requires custom request context handling.

### Acceptance Criteria

- `GBE_DispatchDotaPostLoginRequest` is significantly shorter and easier to scan.
- Adding a new simple handler requires adding a table entry.
- Existing log behavior is preserved.
- Offline GC tests pass.

## Phase 3.4: Centralize Lobby State Transition Rules

### Scope

Consolidate scattered lobby state transition logic into small testable functions.

### Work

- Extract pure transition decision helpers before introducing State Pattern.
- Define transition inputs and outputs explicitly.
- Cover launch, teardown, reconnect, owner disconnect, member disconnect, and stale state regression cases.
- Keep mutation in coordinator methods; keep transition decisions pure where possible.

### Acceptance Criteria

- State transition decision logic is concentrated in one small module.
- Tests cover valid and invalid transitions.
- Existing lobby replay fixtures pass unchanged.
- No broad class hierarchy is introduced during this phase.

## Phase 3.5: Tighten Includes And Internal Boundaries

### Scope

Reduce compile-time coupling introduced by mechanical splits.

### Work

- Replace copy-pasted include blocks with minimal includes per file.
- Forward declare types where safe.
- Move repeated `using` aliases into one internal header only if they are genuinely shared.
- Keep `gbe_dota_gc_internal.h` limited to cross-TU declarations.
- Move file-local helpers into anonymous namespaces.

### Acceptance Criteria

- New split files do not copy the full legacy include block.
- `gbe_dota_gc_internal.h` stops growing as a generic dumping ground.
- A clean build succeeds on the supported local toolchain.
- Offline GC tests pass.

## Recommended Execution Order

1. Phase 3.0 stabilization.
2. Phase 3.1 handler split.
3. Phase 3.2 payload helper split.
4. Phase 3.5 include and boundary cleanup for files touched by 3.1 and 3.2.
5. Phase 3.3 dispatch table after handler signatures are easier to normalize.
6. Phase 3.4 state transition consolidation after tests cover current state behavior.

## Success Metrics

- `gbe_dota_handlers.cpp` under 1500 lines (or under 2000 with documented reason).
- `gbe_dota_gc_payload_helpers.cpp` under 1200 lines.
- Audit script reports zero real high-risk findings.
- Offline GC tests pass after every phase.
- A handler-level test harness covers at least the logic-refactored handlers.
- New pure helper code has focused tests.
- Include lists are meaningfully smaller in newly touched files.
- "Good enough" stop condition: when a file is within 20% of its target and further splitting would require introducing abstractions not yet justified by repeated patterns, document the trade-off and stop. Do not pursue the last 20% at the cost of architectural complexity.

## Risk Management

- Move one responsibility group per commit.
- Run offline GC tests after every move.
- Compare replay fixture outputs before and after logic movement.
- Avoid signature changes and behavior changes in the same commit.
- Keep heavier abstractions behind evidence from repeated patterns.
