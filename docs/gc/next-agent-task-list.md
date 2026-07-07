# Next Agent GC Refactor Task List

This checklist is for the next agent continuing Dota GC / coordinator maintenance work.

The goal is not aesthetic cleanup. Every task must reduce real maintenance risk: make bugs easier to locate, make behavior easier to test, or make review safer. Do not refactor just to reduce line count, hide globals, or split files.

## Current Baseline

Branch: `trae/agent-inRF11`

The local branch currently contains three commits after `b853e797`:

- `056bfeeb refactor(gc): add shared lobby read-only facade helpers`
- `bf5d8896 test(gc): assert shared lobby reads through facade`
- `62bbc688 docs(gc): plan shared lobby clear facade`

These commits were verified locally before handoff:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

Expected important verification signals:

- payload helper tests pass.
- handler smoke tests pass.
- GC audit reports 0 issues.

The root directory may contain unrelated untracked analysis files. Do not stage them:

- `.gitnexus/`
- `.linghun/`
- `ARCADE_BUG_FULL_ANALYSIS.md`
- `GITNEXUS_INDEX_REPORT.md`
- `GRAPH_BUG_INVESTIGATION.md`
- `compute_427.py`
- `extract_template.py`
- `parse_*.py`
- `simulate_427.py`

Verification may generate a local Windows-style log file in the repository root:

```bash
./C:\Users\Public\gbe_gc_debug.log
```

Delete it after verification if present:

```bash
rm -f './C:\Users\Public\gbe_gc_debug.log'
```

## Required Guardrails

Always follow these rules:

1. Test first or test alongside the change.
2. Keep each PR / commit focused on one kind of boundary.
3. Do not mix shared-state facade work, side-effect seam work, and object extraction in one change.
4. Do not change publish, clear, lifecycle, or server/client ownership semantics unless a focused test proves the intended behavior.
5. Do not mechanically wrap every `GBE_shared_dota_lobby_state.foo` read/write.
6. Do not split `Steam_Game_Coordinator` into Dota sub-objects yet.
7. After any GC code change, run at least:

```bash
bash tools/run_gc_verification.sh --fast
git diff --check
```

For lifecycle, clear, server/client ownership, or side-effect ordering changes, run:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

## Task 1. Side-Effect Recorder Coverage Phase 3

Priority: highest.

Purpose: make message ordering and side-effect order visible before introducing broader side-effect wrappers.

### Why This Matters

Many Dota handlers mutate state and emit responses, server-GC forwards, lobby snapshot refreshes, details updates, or network broadcasts. The risk is not that code is long. The risk is accidentally changing protocol order while refactoring.

### Files To Inspect

- `tools/gbe_dota_handler_test/stubs.h`
- `tools/gbe_dota_handler_test/smoke_test.cpp`
- `dll/gbe_dota_lobby_handlers.cpp`
- `dll/gbe_dota_chat_handlers.cpp`
- `dll/gbe_dota_match_handlers.cpp`
- `dll/gbe_dota_inventory_handlers.cpp`
- `dll/gbe_dota_lobby_flow_coordinator.cpp`

### What To Do

Add focused handler smoke assertions for one or two high-risk ordering paths.

Good candidate paths:

1. Normal signout finalize after cache unsubscribe.
   - Prove cache unsubscribe response / pending finalize state is preserved.
   - Prove settings lobby clear and runtime reset ordering is not changed if the test harness exposes it.

2. Lobby destroy.
   - Existing test checks 25 then 8247 response envelopes.
   - Strengthen only if useful: preserve wrapped/session/source job metadata, and prove local clear happens after queued responses if the harness can observe that safely.

3. Server-GC forward / network broadcast / lobby snapshot refresh.
   - Pick an existing path with multiple observable side effects.
   - Assert sequence using `tf.recorder.actions`.
   - Preserve emsg, reason string, target steam id, wrapped/session/source-job metadata when applicable.

4. Rich presence or settings side effects.
   - Only add recorder observation first.
   - Do not wrap production behavior in this task.

### How Far To Go

Complete only a small coverage increment:

- Add recorder fields only if needed by the focused test.
- Add at most one or two new tests, or strengthen one or two existing tests.
- Do not introduce a new `DotaGcSideEffects` interface yet.
- Do not refactor production side-effect execution in the same commit.

### Done When

- The new/strengthened tests fail if the protected order is reversed.
- `bash tools/run_gc_verification.sh --full` passes.
- `git diff --check` passes.
- `docs/gc/follow-up-task-list.md` records exactly what ordering is now protected.

### Stop If

- The test requires heavy production linkage or broad fake SDK changes.
- The change starts wrapping unrelated side effects.
- The diff makes review harder than the original code.

## Task 2. Shared Lobby State Read-Only Facade Phase 3

Priority: high, but only after choosing a narrow read-only cluster.

### Why This Matters

`GBE_shared_dota_lobby_state` is an implicit protocol between client GC, server GC, lifecycle cleanup, postgame, reconnect, and lobby publishing. A facade helps only when it names meaningful access. Mechanical getter/setter replacement is risky noise.

### Existing Helpers

Use existing helpers when they fit exactly:

```cpp
bool GBE_HasSharedDotaLobbyState();
uint64 GBE_GetSharedDotaLobbyIdOrZero();
uint64 GBE_GetSharedDotaGenericLobbyIdOrZero();
GBE_DotaReconnectSharedStateSnapshot GBE_GetSharedDotaReconnectStateSnapshot();
bool GBE_IsSharedDotaArcadeLobbyActive();
```

### Files To Inspect

- `dll/gbe_dota_gc_internal.h`
- `dll/gbe_dota_gc_payload_helpers.cpp`
- `dll/gbe_dota_lobby_state_coordinator.cpp`
- `dll/gbe_dota_lobby_flow_coordinator.cpp`
- `dll/gbe_dota_lobby_launch_coordinator.cpp`
- `tools/gbe_dota_gc_payload_helpers_test/gbe_dota_gc_payload_helpers_test.cpp`
- `tools/gbe_dota_handler_test/smoke_test.cpp`
- `tools/gbe_dota_handler_test/free_func_stubs.cpp`
- `tools/gbe_dota_handler_test/stubs.h`

### What To Do

1. Search remaining direct reads:

```bash
rg 'GBE_shared_dota_lobby_state\.(valid|lobby_id|generic_lobby_id|active|state|game_state)' dll tools
```

2. Classify each read:

- safe read-only scalar read.
- snapshot-style read that already has or deserves a snapshot helper.
- publish/update mutation path.
- lifecycle clear/preserve decision.

3. Replace only one clearly read-only cluster.

Good candidates:

- test assertions that should consume existing facade helpers.
- payload helper reads where a scalar helper exactly preserves current semantics.
- local call sites where invalid state should clearly map to zero/false, matching existing helper semantics.

Bad candidates:

- launch suppression paths that may intentionally inspect stale/raw lobby id.
- normal signout fallback paths that may use a consumed id or raw shared id.
- publish/update paths that mutate `connect`, `server_id`, or state fields.
- any path where `valid == false` but stale field contents might be meaningful.

### How Far To Go

One cluster only. Add tests proving equivalence.

If a new helper is needed, it must be read-only and must not return mutable state. Prefer scalar or immutable snapshot helpers.

If handler tests compile handler TUs without the production helper TU, update:

- `tools/gbe_dota_handler_test/free_func_stubs.cpp`
- `tools/gbe_dota_handler_test/stubs.h`

### Done When

- The changed call site has equivalent behavior.
- Tests cover invalid and valid shared state if helper semantics depend on `valid`.
- No publish, clear, or lifecycle-specific mutation path changed.
- `bash tools/run_gc_verification.sh --full` passes.
- `git diff --check` passes.
- `docs/gc/follow-up-task-list.md` records the completed cluster and explicitly says no publish/clear/lifecycle semantics changed.

### Stop If

- You need to thread a large snapshot through many functions.
- You cannot explain whether invalid state should return zero/false or preserve raw stale data.
- The change starts to look like mechanical getter replacement.

## Task 3. Shared Lobby State Clear Semantic Wrapper Prep

Priority: medium.

### Why This Matters

The raw clear helper already exists and must remain behavior-equivalent:

```cpp
void GBE_ClearSharedDotaLobbyState()
{
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
}
```

The next value is not changing this helper. The value is naming lifecycle intent around it, one path at a time, after tests prove behavior.

### Files To Inspect

- `docs/gc/shared-lobby-state-clear-plan.md`
- `dll/steam_game_coordinator.cpp`
- `dll/gbe_dota_lobby_flow_coordinator.cpp`
- `dll/gbe_dota_lobby_state_coordinator.cpp`
- `tools/gbe_dota_handler_test/smoke_test.cpp`

### What To Do

Pick one lifecycle clear intent and prepare it.

Preferred first candidate:

```cpp
GBE_ClearSharedDotaLobbyForRuntimeReset()
```

Only introduce it if the current runtime reset behavior is already protected or can be protected cheaply.

Steps:

1. Confirm current runtime reset behavior:
   - local lobby cleared.
   - shared lobby state cleared.
   - last pushed launch state cleared.

2. Strengthen or reuse:

```cpp
test_lobby_runtime_reset_clears_local_shared_and_last_launch_state
```

3. Add a behavior-equivalent wrapper only if it makes the lifecycle intent clearer:

```cpp
void GBE_ClearSharedDotaLobbyForRuntimeReset()
{
    GBE_ClearSharedDotaLobbyState();
}
```

4. Replace only the runtime reset call site.

### How Far To Go

One wrapper, one call site. No preserve logic. No logging. No extra clearing. No reason strings unless there is an existing audit/test pattern covering them.

### Done When

- Wrapper is behavior-equivalent.
- Runtime reset test protects local/shared/launch-state reset behavior.
- Full verification passes.
- Docs record the wrapper and explicitly say it is behavior-equivalent.

### Stop If

- The wrapper starts taking flags.
- The wrapper changes server/client ownership behavior.
- The wrapper combines local lobby clearing, shared clearing, settings clearing, and rich presence in one helper without tests.

## Task 4. Settings / Rich Presence / Server-Client Lookup Boundaries

Priority: medium, after recorder coverage improves. Settings clear and launch-state target-selection now have narrow seams, so avoid repeating those first steps.

### Why This Matters

Settings and host/client target lookup are maintenance risks because they cross ownership boundaries. Bugs here often look like postgame cleanup, reconnect, or lobby ownership bugs.

### Candidate Helpers

Only add behavior-equivalent or read-only helpers first:

```cpp
bool GBE_HostHasActiveDotaServerLobby(uint64 lobby_id) const;
// Or a future launch-state restore/capture/settings seam with a small explicit context.
```

Some of these already exist or have partial pure-helper coverage. Prefer extending tests around existing seams before adding new ones.

### Files To Inspect

- `dll/gbe_dota_lobby_flow_coordinator.cpp`
- `dll/gbe_dota_lobby_state_coordinator.cpp`
- `dll/gbe_dota_lobby_launch_coordinator.cpp`
- `dll/steam_game_coordinator.cpp`
- `tools/gbe_dota_handler_test/stubs.h`
- `tools/gbe_dota_handler_test/smoke_test.cpp`

### What To Do

Pick exactly one boundary:

1. Future launch-state restore/capture/settings/logging seam after the current planner, payload, and action seams.
2. Another rich-presence clear/update path only if it has a distinct ordering risk from standard or custom 7041 launch.
3. Another host/server ownership seam only if it names a different tested predicate.

Add or strengthen a test first. Then add a helper only if it makes the ownership side effect more visible.

Current launch-state status:

- `plan_launch_state_push(...)` already covers target-selection, shared suppression, capture availability, launch eligibility, duplicate suppression, push action booleans, and owner-LAN preserve predicates.
- Launch-state push now has narrow seams for target/shared/captured plan-input mapping, payload build requests, grouped payload builds, and action-sequence execution.
- Full focused production harness remains deferred because `gbe_dota_lobby_launch_coordinator.cpp` still groups launch-state push with unrelated launch/teardown/response members that conflict with existing handler smoke stubs.
- Continue only if the next step keeps the explicit context small and does not link the full launch coordinator TU into handler smoke tests.

### How Far To Go

One boundary only. Do not combine settings, rich presence, and server/client lookup in one change.

### Done When

- The helper is read-only or behavior-equivalent.
- Existing order and ownership behavior is unchanged.
- Full verification passes.
- Docs explain which ownership boundary is now named.

### Stop If

- The helper needs to know too much about unrelated lifecycle phases.
- You need broad changes to `Steam_Game_Coordinator` construction.
- You are tempted to extract a Dota sub-object.

## Task 5. Push / Response / Broadcast Seam

Priority: medium-low until recorder coverage is stronger.

### Why This Matters

The eventual goal is to make side effects easier to review. But wrapping too early hides ordering. Tests must come first.

### Candidate Side Effects

- `push_incoming_now(...)`
- `push_incoming_response(...)`
- lobby details update
- server GC forward
- network broadcast
- lobby snapshot refresh
- rich presence update

### What To Do

Only after recorder tests prove order for a path:

1. Choose one repeated side-effect operation.
2. Create a named helper that preserves the exact call and metadata.
3. Replace one or two call sites.
4. Verify message order and metadata remain unchanged.

### How Far To Go

No broad `DotaGcSideEffects` interface yet unless the existing tests already cover all affected operations. A small helper is better than a premature interface.

### Done When

- The helper makes review easier.
- A test proves order and metadata.
- Full verification passes.

### Stop If

- The helper hides whether a response was immediate, delayed, wrapped, or tied to a source job.
- The change touches more than one handler family.

## Task 6. Inventory / VPK / Template Replay Data Flow

Priority: lower unless active bugs appear.

### Candidate Areas

- inventory equip action-list flow.
- style unlock / consumable ordering.
- VPK loot data loading and item generation.
- Dota template replay / identifier patching.
- `parse_dota7034_runtime_request` and runtime payload adaptation.

### What To Do

Only work here when there is a bug, test gap, or repeated review pain.

Preferred work:

- add focused fixture tests.
- make pure payload helpers explicit.
- preserve binary payload output exactly.

### Stop If

- The change is only moving code between files.
- The test fixture cannot prove payload equivalence.

## Task 7. Dota Sub-Object Extraction

Priority: defer.

Do not do this yet.

Only reconsider after:

- shared-state read/clear/publish boundaries are named.
- side-effect order is covered by recorder tests.
- settings/rich-presence/server-client lookup seams are stable.
- launch-state planner, payload build, and side-effect execution seams remain small and stable.
- repeated methods naturally group around a small state surface.

A good extraction candidate must own a coherent state and dependency boundary. A bad extraction just moves `Steam_Game_Coordinator` methods into another class that still reaches back into the same globals and side effects.

## Suggested Order For The Next Agent

Do these in order unless a concrete bug dictates otherwise:

1. Run baseline verification:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

2. Check `docs/gc/follow-up-task-list.md` and `docs/gc/lifecycle-state-map.md` for the latest coverage status.
3. Pick one remaining boundary only if it has a focused test path: rich presence clear/update ordering, a future launch-state restore/capture/settings/logging seam with a small explicit context, or another host/server ownership seam with a different tested predicate.
4. Stop if the change needs broad `Steam_Game_Coordinator` construction, a wider handler smoke harness, or a Dota sub-object extraction.
5. Commit the small step with verification results, then hand off with status and remaining risk.

## Handoff Format

At handoff, report:

- commits created.
- exact files changed.
- tests run and result.
- whether any root `C:*` log was cleaned.
- whether untracked analysis files were left untouched.
- next recommended task.
