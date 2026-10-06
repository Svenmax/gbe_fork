# GC Launch Runtime 单写入实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 queued lobby state 路径的 `state`、`game_state` 与 `launch_phase` 更新收敛为单个可测试 apply 边界，同时保留 generation 和协议顺序。

**Architecture:** `compose_queued_lobby_state_apply_plan()` 继续生成纯字段计划。新增或复用一个仅修改 Local 工作副本的 apply 函数，`Steam_Game_Coordinator::GBE_ApplyQueuedLobbyState()` 通过该函数应用字段组，再保持既有 rich presence、peer push 和 shared publish 顺序。

**Tech Stack:** C++17、现有 `GBE_LocalLobby`、Dota GC offline tests、Python 审计。

## Global Constraints

- `GBE_local_lobby` 保持当前 GC 的 Local 工作副本。
- shared Store 生产写继续经 `GBE_PublishSharedDotaLobbyState` 与 generation gate。
- host-only shared publish 和 client restore 语义保持。
- 不修改 members、chat、`owner_hero_id`、`server_id`、`connect`、`match_id`。
- 不引入生产 CompositionRoot，不拆分 `Steam_Game_Coordinator`，不改变 response/push/publish 顺序。
- 每个任务完成后运行 `bash tools/run_gc_verification.sh --full` 与 `git diff --check`。

---

### Task 1: 纯 queued launch/runtime apply 边界

**Files:**
- Modify: `dll/gbe_dota_lobby_state.h:199-204`
- Modify: `dll/gbe_dota_lobby_state.cpp:314-345`
- Test: `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp:708-750`

**Interfaces:**
- Consumes: `QueuedLobbyStateApplyPlan` 和 `GBE_LocalLobby`。
- Produces: `void apply_queued_lobby_state_apply_plan(GBE_LocalLobby &, const QueuedLobbyStateApplyPlan &)`。

- [ ] **Step 1: 写失败测试**

在 `gbe_dota_lobby_state_test.cpp` 增加以下场景：

```cpp
GBE_LocalLobby lobby{};
lobby.state = 1u;
lobby.game_state = 2u;
lobby.launch_phase = 1u;
const auto plan = gbe::dota_lobby_state::QueuedLobbyStateApplyPlan{2u, 3u, 4u, false};
gbe::dota_lobby_state::apply_queued_lobby_state_apply_plan(lobby, plan);
assert(lobby.state == 2u);
assert(lobby.game_state == 3u);
assert(lobby.launch_phase == 4u);
```

- [ ] **Step 2: 确认测试失败**

运行：`bash tools/run_gc_offline_tests.sh --full`

预期：`gbe_dota_lobby_state_test` 因缺少 `apply_queued_lobby_state_apply_plan` 而构建失败。

- [ ] **Step 3: 实现最小 apply 函数**

在 `gbe_dota_lobby_state.h` 声明：

```cpp
void apply_queued_lobby_state_apply_plan(
    GBE_LocalLobby &lobby,
    const QueuedLobbyStateApplyPlan &plan);
```

在 `gbe_dota_lobby_state.cpp` 实现：

```cpp
void apply_queued_lobby_state_apply_plan(
    GBE_LocalLobby &lobby,
    const QueuedLobbyStateApplyPlan &plan)
{
    lobby.state = plan.state;
    lobby.game_state = plan.game_state;
    lobby.launch_phase = plan.launch_phase;
}
```

- [ ] **Step 4: 验证 focused 测试**

运行：`bash tools/run_gc_offline_tests.sh --full`

预期：`gbe_dota_lobby_state_test passed`。

- [ ] **Step 5: 提交**

```bash
git add dll/gbe_dota_lobby_state.h dll/gbe_dota_lobby_state.cpp tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp
git commit -m "refactor(gc): centralize queued lobby state apply"
```

### Task 2: Coordinator 使用单写入入口

**Files:**
- Modify: `dll/steam_game_coordinator.cpp:327-372`
- Test: `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp`
- Test: `tools/gbe_dota_handler_test/smoke_test.cpp`

**Interfaces:**
- Consumes: `apply_queued_lobby_state_apply_plan(GBE_LocalLobby &, const QueuedLobbyStateApplyPlan &)`。
- Produces: queued lobby state 路径对 launch/runtime 字段的唯一 apply 调用。

- [ ] **Step 1: 写顺序回归测试**

在现有 queued-state 或 launch-state smoke 场景中断言：state apply 发生在 rich presence、peer push 和 shared publish 之前；断言 outbound emsg 序列与现有期望一致。

- [ ] **Step 2: 确认测试保护当前顺序**

运行：`bash tools/run_gc_offline_tests.sh --full`

预期：handler 测试显示 `90/90`，新断言通过当前实现。

- [ ] **Step 3: 替换 Coordinator 直接字段写入**

在 `Steam_Game_Coordinator::GBE_ApplyQueuedLobbyState()` 中替换：

```cpp
GBE_local_lobby.state = apply_plan.state;
GBE_local_lobby.game_state = apply_plan.game_state;
GBE_local_lobby.launch_phase = apply_plan.launch_phase;
```

为：

```cpp
gbe::dota_lobby_state::apply_queued_lobby_state_apply_plan(GBE_local_lobby, apply_plan);
```

保持日志、`GBE_ReapplyDotaPracticeLobbyLaunchRichPresence()`、`GBE_PushDotaLaunchStateToClientPeer()` 与 `GBE_PublishSharedDotaLobbyState()` 的现有位置。

- [ ] **Step 4: 运行完整验证**

运行：`bash tools/run_gc_verification.sh --full`

预期：audit、payload helper `555/555`、handler `90/90`、reconnect `772/772` 均通过。

- [ ] **Step 5: 提交**

```bash
git add dll/steam_game_coordinator.cpp tools/gbe_dota_handler_test/smoke_test.cpp
git commit -m "refactor(gc): apply queued launch state through boundary"
```

### Task 3: 更新双轨写入清单和受控切片状态

**Files:**
- Modify: `docs/gc/LOCAL_LOBBY_USAGE.md:42-53`
- Modify: `docs/gc/ACTIVE_QUEUE.md:6-30`
- Modify: `docs/gc/PHASE_D_BOUNDARY.md:47-53`
- Modify: `docs/gc/REFACTOR_EXECUTION_PLAN.md`

**Interfaces:**
- Consumes: Task 1 和 Task 2 的 apply 边界。
- Produces: 真实写入口清单与 Phase D 受控切片状态。

- [ ] **Step 1: 更新字段所有权记录**

在 `LOCAL_LOBBY_USAGE.md` 的 launch/runtime 条目中记录：queued lobby state 路径经 `compose_queued_lobby_state_apply_plan()` 和 `apply_queued_lobby_state_apply_plan()` 一次应用 `state`、`game_state` 与 `launch_phase`。

- [ ] **Step 2: 更新受控切片状态**

在 `ACTIVE_QUEUE.md` 写入完成记录；在 `PHASE_D_BOUNDARY.md` 将当前受控切片描述为 launch/runtime 单写入的第一条 queued-state 路径，不改变 Phase D 默认停手条件。

- [ ] **Step 3: 验证文档与代码**

运行：`git diff --check && bash tools/run_gc_verification.sh --full`

预期：命令成功退出。

- [ ] **Step 4: 提交**

```bash
git add docs/gc/LOCAL_LOBBY_USAGE.md docs/gc/ACTIVE_QUEUE.md docs/gc/PHASE_D_BOUNDARY.md docs/gc/REFACTOR_EXECUTION_PLAN.md
git commit -m "docs(gc): record queued launch state boundary"
```

## 自检

- R1 由 Task 1 和 Task 2 的 pure apply 边界与 Coordinator 调用覆盖。
- R2 由 Task 2 保持既有 publish 位置覆盖。
- R3 由 Task 2 的 smoke 顺序断言和完整 offline suite 覆盖。
- R4 由每个任务的 focused/完整验证覆盖。
- 计划未包含生产 DI、Coordinator 大拆、host 字段迁移或真协议 L2/L4。
