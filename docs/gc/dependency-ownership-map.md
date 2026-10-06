# Dota GC Dependency And Ownership Map

This map turns the current coordinator mix into explicit dependency areas. Use it before starting another extraction pass.

## Dependency Areas

| Area | Current owner | Existing seams and tests | Extraction readiness | Next useful step |
| --- | --- | --- | --- | --- |
| Launch-state push decisions | `GBE_PushDotaLaunchStateToClientPeer(...)` plus `gbe::dota_lobby_flow` | Planner, source/target/shared/capture/captured-context mapping, payload build requests, grouped payload builds, and action sequence helpers are tested. | Medium. Decision and action sequencing are stable; restore, capture source, and logging still live in the coordinator. | Wait for a small explicit context around restore, capture source, or logging. |
| Response push | `GBE_PushDotaResponse(...)` and narrow response helpers | Response helper status is documented in `response-seam-status.md`; recorder coverage exists for selected `25`, `7014`, `7055`, and `7010` call sites. | Medium. Helpers clarify reviewed side effects, but each member helper must be mirrored in handler smoke stubs. | Stop mechanical helper growth; move only covered high-risk call sites or design a small response facade after stub cost is addressed. |
| Shared lobby state read | `GBE_GetSharedDotaLobbyStateSnapshot()` and value-derived helpers | Business operations capture one complete snapshot; focused and concurrent store tests cover copy isolation and complete-version reads. | High. Production callers have no mutable backing-state access. | Keep one snapshot per business operation and preserve invalid-state behavior. |
| Shared lobby mutation and clear | `gbe::dota_lobby_state::Store` through the production accessor | Complete publish, monotonic generation publish, compare/update, stale rejection, and clear have focused tests. Audit 10 prevents direct production access to the retired global symbol. | High. The backing DTO is private to the store accessor and retains the existing `global_mutex` synchronization domain. | Add lifecycle-specific store operations only when a repeated semantic boundary appears. |
| Lobby publish and details update | Handler flows plus `GBE_SendDotaPracticeLobbyDetailsUpdate(...)` | Recorder covers mutation before publish, publish before details update, and selected details-update reasons. | Medium. Details update is visible, while publish still carries local/shared state coupling. | Extract only one publish/detail path after recorder coverage proves reason and order. |
| Settings and rich presence | Handler flows and coordinator methods | Recorder covers settings sync and multiple rich presence ordering paths. | Low-medium. Calls are still intertwined with lifecycle phase decisions. | Add small lifecycle-intent helpers only when they preserve existing order and reasons. |
| Server/client coordinator lookup | Coordinator methods and Steam client access | Active-lobby ownership predicate is tested; launch target choice is covered by flow peer-selection and target-mapping tests. | Low-medium. Runtime lookup still depends on coordinator construction and handler harness stubs. | Add predicates only for distinct ownership decisions; avoid broad constructor changes. |
| Inventory, template replay, and misc responses | Domain handlers and pure payload helpers | Payload helper and replay tests cover many binary payload contracts. | Low priority. These areas are less tied to current lifecycle ownership risk. | Work here only for bugs, fixture gaps, or repeated review pain. |

## Extraction Gate

A Dota sub-object extraction should wait until the target area can be constructed from a small dependency set. Avoid passing the whole coordinator, raw settings object, shared lobby globals, network, and response push machinery into a new object at once.

## Recommended Phase Boundary

Treat the current work as the end of the seam-strengthening phase. The next phase should start with a focused design pass for one facade, most likely response push or launch-state service, and should include the constructor dependencies, test harness impact, and stop conditions before touching production code.
