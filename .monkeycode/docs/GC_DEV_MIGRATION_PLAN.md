# Dota GC Dev Migration Plan

## Current Branch Facts

- Working branch: `trae/agent-inRF11`
- Tracking branch: `origin/trae/agent-inRF11`
- Delivery range for the current local work: `origin/trae/agent-inRF11..HEAD`
- Current local work: 107 commits, 48 files, 4817 insertions, 619 deletions
- Pre-existing GC extraction baseline: `origin/dev..origin/trae/agent-inRF11`
- Pre-existing baseline size: 51 commits, 165 files, 56047 insertions, 20057 deletions
- `origin/dev` and `trae/agent-inRF11` do not share a merge-base in this checkout.
- Current branch root commit: `c5dd26d4 fix(gc): harden dota payload helpers after refactor review`
- `origin/dev` root commit: `d968c3e1 Initial commit.`

## Migration Check Result

Generated patch files under `/tmp/opencode/gbe-migration-check-20260707/`:

- `gc-refactor.patch`
- `files.txt`
- `stat.txt`

Created a detached `origin/dev` worktree at `/tmp/opencode/gbe-migration-check-20260707/dev-worktree`.

Commands executed:

```bash
git apply --check /tmp/opencode/gbe-migration-check-20260707/gc-refactor.patch
git apply --3way --check /tmp/opencode/gbe-migration-check-20260707/gc-refactor.patch
```

Both checks failed. The current 107-commit patch cannot be applied directly to `origin/dev`.

## Primary Failure Classes

- `origin/dev` is missing the earlier GC extraction baseline files that the current work depends on.
- Existing core files on `origin/dev` have incompatible context for this patch.
- Three-way apply can identify some conflict locations, but cannot resolve missing indexed files for the split GC translation units and test harnesses.

Representative missing or conflicting files:

- `dll/gbe_dota_action_model.h`
- `dll/gbe_dota_lobby_flow.cpp`
- `dll/gbe_dota_lobby_flow.h`
- `dll/gbe_dota_lobby_handlers.cpp`
- `dll/gbe_dota_lobby_launch_coordinator.cpp`
- `dll/gbe_dota_lobby_state.cpp`
- `dll/gbe_dota_lobby_state_coordinator.cpp`
- `tools/gbe_dota_handler_test/smoke_test.cpp`
- `tools/gbe_dota_lobby_flow_test/gbe_dota_lobby_flow_test.cpp`
- `tools/run_gc_offline_tests.sh`

## Required Migration Stages

### Stage 1: Establish GC Extraction Baseline on Dev

Bring the 51-commit pre-existing extraction baseline from `origin/trae/agent-inRF11` into a `dev`-based branch. This baseline includes the handler extraction, GC wire/payload helpers, dispatch table, coordinator seams, offline tests, replay fixtures, audit script, and source-list updates.

Representative baseline areas:

- `dll/gbe_dota_*` split translation units
- `dll/gbe_gc_*` helpers
- `dll/gbe_proto_*` wire helpers
- `tools/gbe_dota_*_test` harnesses
- `tools/gc_replay_test`
- `tools/run_gc_offline_tests.sh`
- `tools/run_gc_verification.sh`
- `premake5.lua` GC test/source list entries
- `docs/gc/*`

### Stage 2: Verify Baseline on Dev

After Stage 1, run the GC verification gate before applying the current 107-commit work.

```bash
tools/run_gc_verification.sh
git diff --check
git submodule foreach 'git status --porcelain'
```

### Stage 3: Apply Current Architecture Closure Work

Apply the current delivery range `origin/trae/agent-inRF11..HEAD` on top of the verified Stage 1 baseline.

Main areas:

- Launch push context/planner/action-list/executor consolidation
- Create/join lobby context and action-list consolidation
- Abandon/signout/leave/postgame teardown action-list consolidation
- `gbe_dota_lobby_flow` split into launch/member/chat/payload flow units
- Handler smoke tests, planner tests, replay fixture updates, and audit guardrail updates

### Stage 4: Final Verification

Run the same verification gate after Stage 3.

```bash
tools/run_gc_verification.sh
git diff --check
git submodule foreach 'git status --porcelain'
```

## Recommended Delivery Strategy

- Preserve the current branch by pushing `trae/agent-inRF11` when authorized.
- Treat `dev` integration as a dedicated migration task.
- Avoid using `origin/dev...HEAD` for review because there is no merge-base.
- Use `origin/trae/agent-inRF11..HEAD` to review the current completed architecture closure work.
