# GC Architecture Investment Gates

## Purpose

This record controls optional architecture investments after the explicit lifecycle state-machine phase. A gate opens only from recorded repository evidence. Major concurrency or lifecycle expansions must refresh the inputs before selecting an implementation.

## Actor Gate

An Actor or single-state-owner migration is triggered when any condition below is present:

1. ThreadSanitizer reports a reproducible race in lobby lifecycle, reconnect, callback delivery, or delayed GC work after the existing owner and lock contract has been applied.
2. A production operation requires more than the documented two-lock order of process `global_mutex` followed by one serialized-instance mutex, or requires cyclic/reversed acquisition to remain correct.
3. Two confirmed defects within one delivery phase are caused by out-of-order callbacks mutating the same lifecycle state after generation validation.
4. Production business state gains a direct multi-thread writer outside `gbe::dota_lobby_state::Store`, the callback queue, the coordinator pending-message queue, or a generation-guarded deferred slot.

When triggered, create a dedicated implementation checklist before changing ownership. The checklist must define the single state owner, message vocabulary, queue backpressure, shutdown behavior, callback adaptation, ordering compatibility, and migration tests.

When no condition is present, retain the current Store, generation, queue, and explicit lock model.

## Current Actor Decision

Decision date: 2026-07-11.

| Input | Current evidence | Result |
| --- | --- | --- |
| TSAN | Clang `tools/run_gc_tsan_tests.sh` passes reconnect 772/772 and the bounded concurrency stress test with zero race reports. | Clear |
| Lock complexity | The production reconnect order remains `global_mutex -> serialized instance mutex -> prepare/dedup reservation -> release instance mutex -> release global_mutex -> external effects`. Store operations use the existing recursive process lock. | Clear |
| Out-of-order callback defects | Generation guards reject stale reconnect callbacks, delayed GC messages, runtime updates, and deferred lifecycle slots. No confirmed post-P11 defect record identifies two out-of-order mutation failures in one phase. | Clear |
| Cross-thread business-state writers | Shared lobby writes use `Store`; callback and delayed work use copied queue entries with final generation validation; serialized reconnect state remains instance-local. Audit 16 reports no unapproved mutable GC business globals. | Clear |

The Actor gate is closed. The current Store, generation, queue, and explicit lock model remains the selected architecture.

## Formal Model Gate

A maintained TLA+ or Alloy model is triggered when at least two conditions below are present, or when the critical-failure condition is present:

1. The canonical lifecycle model exceeds 16 states, 24 event kinds, or 384 state/event pairs after unreachable combinations are removed.
2. Two confirmed defects within one delivery phase are caused by duplicate, stale, or out-of-order lifecycle events and escape the existing example, property, differential, and production-path tests.
3. A lifecycle failure can irreversibly corrupt persisted user data, cross account or process isolation boundaries, or produce an unrecoverable remote protocol state. This critical-failure condition opens the gate independently.
4. Three or more active maintainers must change the lifecycle transition contract during the same release cycle, creating sustained coordination pressure around invariants.

When triggered, the formal model scope is limited to lobby lifecycle state, generation advancement and validation, and reconnect deduplication. Side-effect payloads, Steam transport details, inventory, chat, and unrelated GC messages remain outside the model.

The model deliverable must define its maintained owner, checked invariants, bounded assumptions, tool version, local command, CI cost, and correspondence with the C++ transition vocabulary before implementation starts.

## Current Formal Model Decision

Decision date: 2026-07-11.

| Input | Current evidence | Result |
| --- | --- | --- |
| State-space size | The canonical model has 8 states, 13 event kinds, and 104 state/event pairs. Compile-time completeness classifies every pair. | Below threshold |
| Duplicate and ordering defects | Example tests cover duplicate and out-of-order loading; properties exhaust 100,000 length-five lifecycle sequences and every stale-generation state/event pair; no post-P14 escaped defect is recorded. | Clear |
| Failure cost | Rejected transitions preserve state and emit no effects. Generation guards discard stale asynchronous work. The modeled lifecycle does not own persisted inventory or account authorization. | Recoverable scope |
| Maintenance pressure | The transition contract is centralized in one dependency-light header with one test owner and one production effect-gate audit. No three-maintainer concurrent-change record exists. | Clear |

The formal model gate is closed. The current constexpr transition table, compile-time completeness checks, deterministic properties, and differential reference model remain the selected verification strategy.
