# GC Structural Guardrails And Replay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace fragile Local lobby regex guard growth with compiler-enforced ownership, add production TU build coverage, and ground replay tests in real capture scenarios.

**Architecture:** Keep `GBE_LocalLobby` as the protocol DTO, but move mutable ownership behind a dedicated owner boundary instead of letting every `Steam_Game_Coordinator` member function write fields directly. Keep offline tests, Python audits, production TU syntax checks, and real capture replay as layered gates.

**Tech Stack:** C++17, Bash, Python 3, protobuf `protoc`, existing GC replay and offline test tools.

## Global Constraints

- Stop adding new `GBE_local_lobby` syntax-variant regex guard commits.
- Every production GC `.cpp` must be directly syntax-checked with generated protobuf headers.
- Deletion of docs, empty TUs, and archived source snapshots requires explicit reviewer confirmation.
- `bash tools/run_gc_verification.sh --full` is the required verification gate for each production slice.
- Preserve response, push, publish, and deferred work ordering.

---

### Task 1: Production GC TU Gate

**Files:**
- Create: `tools/check_gc_production_tus.sh`
- Modify: `tools/run_gc_verification.sh`
- Modify: `.github/workflows/emu-pull-request.yml`
- Modify: GC production `.cpp` files that miss direct includes

**Interfaces:**
- Consumes: generated `net.pb.h`, `steammessages.pb.h`, and TF2 protobuf headers.
- Produces: `bash tools/check_gc_production_tus.sh --jobs N`, called by `run_gc_verification.sh --full`.

- [x] **Step 1: Add production TU syntax checker**

Run: `bash tools/check_gc_production_tus.sh --jobs 8 --keep-going`
Expected before fixes: missing direct include and private access failures are reported.

- [x] **Step 2: Fix self-contained include failures**

Add direct `#include "gbe_dota_gc_diagnostics.h"` to split GC TUs using diagnostic functions.
Add direct `#include "gbe_dota_binary_helpers.h"` to `dll/steam_game_coordinator.cpp` for `ser_var`.

- [x] **Step 3: Fix private access in template replay helper**

Convert `GBE_TryHandleDotaRegistryDefensiveTemplateReplay` into a private `Steam_Game_Coordinator` member helper so it can call private handler methods while keeping the audit boundary explicit.

- [x] **Step 4: Run full verification**

Run: `bash tools/run_gc_verification.sh --full`
Expected: offline tests, audit, production TU syntax check, and diff check all pass.

### Task 2: Structural Local Lobby Ownership

**Files:**
- Modify: `dll/dll/steam_game_coordinator.h`
- Modify: `dll/gbe_dota_lobby_state.h`
- Modify: `dll/gbe_dota_lobby_state.cpp`
- Modify: GC coordinator and handler files currently writing `GBE_local_lobby`
- Test: `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp`
- Test: `tools/test_audit_gc_refactor.py`

**Interfaces:**
- Consumes: `GBE_LocalLobby` DTO and existing named apply/restore helpers.
- Produces: a Local lobby owner boundary that exposes read-only snapshots and named mutators.

- [ ] **Step 1: Add owner type with no mutable raw getter**

Add a `LocalLobbyOwner` wrapper that stores `GBE_LocalLobby` privately and exposes:

```cpp
const GBE_LocalLobby &snapshot() const;
void replace_for_reset(GBE_LocalLobby lobby);
template <typename Apply>
decltype(auto) apply(const char *reason, Apply &&apply);
```

- [ ] **Step 2: Migrate one low-risk apply call**

Move `apply_client_lobby_restore_snapshot(...)` behind `LocalLobbyOwner::apply(...)` and prove the production TU check still passes.

- [x] **Step 2a: Remove cross-instance raw Local lobby access**

Route peer reads through `Steam_Game_Coordinator::GBE_PeerLocalLobbySnapshot()` and peer restore writes through `Steam_Game_Coordinator::GBE_ApplyPeerClientLobbyRestoreSnapshot(...)`. Verify with `rg -n "(->|\.)GBE_local_lobby" dll -g '*.cpp' -g '*.h'` returning no matches.

- [ ] **Step 3: Replace regex-growth audit with structural audit**

Keep one audit that rejects new direct `GBE_local_lobby` field writes, but stop adding syntax-specific variants. Add an audit that requires new writes to go through `LocalLobbyOwner::apply` or named `gbe::dota_lobby_state::apply_*` helpers.

- [ ] **Step 4: Migrate remaining direct writes in batches**

Batch by owner: lifecycle, lobby create/join/slot, match, inventory, snapshot/restore. Each batch ends with `bash tools/run_gc_verification.sh --full`.

### Task 3: Real Capture Replay Golden Fixtures

**Files:**
- Create: `tools/gc_replay_test/capture_dir_to_fixture.py`
- Modify: `tools/gc_replay_test/README.md`
- Create: selected fixture pairs under `tools/gc_replay_test/fixtures/`

**Interfaces:**
- Consumes: NetHook capture directories containing `*_5452_*` and `*_5453_*` `.bin` files.
- Produces: `gc_replay_test` fixture lines and golden summaries.

- [x] **Step 1: Add capture converter**

Run: `python3 tools/gc_replay_test/capture_dir_to_fixture.py /workspace/gbe-fork-debug-workspace/captures/hostinviteplayerlobby --output /tmp/opencode/hostinviteplayerlobby.fixture`
Expected: a fixture with ordered GC rows and stable labels.

- [x] **Step 2: Add three first golden scenarios**

Use `hostinviteplayerlobby`, `hostlobbykickplayer`, and `steamhostlobbyswapteam` as the first golden set. Generate each expected file with `/tmp/opencode/gc_replay_test <fixture> > <fixture>.expected.txt`.

- [x] **Step 3: Add create/launch/leave scenario coverage**

Select one scenario covering create, launch, and leave from `/workspace/gbe-fork-debug-workspace/captures`. Generate fixture and expected summary.

- [x] **Step 4: Wire golden scenarios into offline tests**

Extend `tools/run_gc_offline_tests.sh` so these golden fixtures run with existing replay tests.

### Task 4: Documentation And Empty TU Cleanup

**Files:**
- Review: `.monkeycode/specs/*`
- Review: `docs/gc/*.md`
- Review: `dll/gbe_dota_lobby_handlers.cpp`
- Review: `dll/gbe_dota_lobby_state_coordinator.cpp`
- Review: `z_original_repo_files/`

**Interfaces:**
- Consumes: current docs and empty shell TUs.
- Produces: a reviewer-approved deletion list and a smaller truth-document set.

- [ ] **Step 1: Produce deletion candidate list**

List exact files/directories to delete and classify each as generated history, empty shell, duplicate status, or archived upstream snapshot.

- [ ] **Step 2: Ask for explicit delete confirmation**

Deletion requires explicit confirmation naming the files/directories.

- [ ] **Step 3: Remove confirmed files and update build/audit references**

After confirmation, remove empty TU references from premake and audit exemption maps.

- [ ] **Step 4: Run full verification**

Run: `bash tools/run_gc_verification.sh --full`
Expected: all gates pass after cleanup.

### Task 5: Dual-Track State Convergence Continuation

**Files:**
- Modify: `dll/gbe_dota_lobby_state.h`
- Modify: `dll/gbe_dota_lobby_state.cpp`
- Modify: `dll/gbe_dota_lobby_state_publish_coordinator.cpp`
- Modify: `dll/gbe_dota_lobby_snapshot_coordinator.cpp`
- Test: `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp`
- Test: `tools/gbe_dota_handler_test/smoke_test.cpp`

**Interfaces:**
- Consumes: existing generic lobby capture plans and Store generation gates.
- Produces: snapshot-only apply boundaries and generation-safe Local/shared convergence.

- [ ] **Step 1: Resume from `2026-07-25-gc-dual-track-state-convergence.md` Task 2**

Write failing tests for snapshot-only capture that must not publish shared state.

- [ ] **Step 2: Implement minimal plan/apply boundary**

Add only the fields required by the failing test, then run focused state tests.

- [ ] **Step 3: Run full gate**

Run: `bash tools/run_gc_verification.sh --full`
Expected: production TU and offline gates pass.
