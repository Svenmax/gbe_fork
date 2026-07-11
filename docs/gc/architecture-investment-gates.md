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
