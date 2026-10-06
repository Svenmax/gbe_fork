# GC Dual Track State Convergence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Dota GC 的 generic capture、shared publish 和 shared restore 收敛为可验证的 Local/shared 双轨状态边界。

**Architecture:** `GBE_local_lobby` 保持为协议处理的工作副本，Store 保持 host 权威共享快照。每个切片先构造纯字段组 plan，再由 coordinator 在明确的 Local apply、snapshot-only 或 host publish 边界执行；restore 使用字段来源和写许可消除同 generation 的隐式覆盖。

**Tech Stack:** C++17、现有 `GBE_LocalLobby`、`gbe::dota_lobby_state::Store`、GC offline test runners。

## Global Constraints

- 生产 shared 写入只经 generation-gated `GBE_PublishSharedDotaLobbyState()` 或既有 Store 门面。
- `GBE_local_lobby` 保持 Local 工作副本；本计划不引入生产 CompositionRoot。
- 每个提交只迁移一个 state 边界，保持 response、push、publish 和 deferred work 顺序。
- 每个切片通过 `git diff --check` 与 `bash tools/run_gc_verification.sh --full` 后进入人工提交检查点。
- members、chat、broadcast、owner_hero_id 的所有权属于独立切片。

---

### Task 1: 完成当前 Custom Game Capture 切片

**Files:**
- Modify: `dll/gbe_dota_lobby_state.h:247-267`
- Modify: `dll/gbe_dota_lobby_state.cpp:515-570`
- Modify: `dll/gbe_dota_lobby_state_publish_coordinator.cpp:156-167`
- Test: `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp:338-389`
- Modify: `docs/gc/ACTIVE_QUEUE.md`

**Interfaces:**
- Consumes: `GenericLobbyCustomGameCapturePlan`。
- Produces: 已验证的 `compose_generic_lobby_custom_game_capture_plan()` 与 `apply_generic_lobby_custom_game_capture_plan()`。

- [ ] **Step 1: 运行 focused state 回归**

Run: `bash tools/run_gc_verification.sh --full`

Expected: 输出 `GC verification passed`。

- [ ] **Step 2: 核对空 raw key 与完整字段组断言**

```cpp
const auto empty_plan = compose_generic_lobby_custom_game_capture_plan(
    "", "", "", "", "", "", "", "", "");
apply_generic_lobby_custom_game_capture_plan(lobby, empty_plan);
```

Expected: Local `custom_game` 既有字段保持原值；完整 raw 输入更新 mode、map、数值字段和 penalties。

- [ ] **Step 3: 更新 Active Queue 状态**

将此状态边界作为单独 D2 切片记录在 `ACTIVE_QUEUE.md`，完成后移入最近完成记录。

- [ ] **Step 4: 执行提交前检查点**

Run: `git diff --check && bash tools/run_gc_verification.sh --full`

Expected: 两个命令均返回成功。保留 diff 供人工提交批准。

### Task 2: Generic Capture 纯计划与 Snapshot-only 边界

**Files:**
- Modify: `dll/gbe_dota_lobby_state.h`
- Modify: `dll/gbe_dota_lobby_state.cpp`
- Modify: `dll/gbe_dota_lobby_state_publish_coordinator.cpp:76-289`
- Modify: `dll/gbe_dota_lobby_snapshot_coordinator.cpp:64-217`
- Test: `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp`
- Test: `tools/gbe_dota_handler_test/smoke_test.cpp`
- Modify: `docs/gc/LOCAL_LOBBY_USAGE.md`

**Interfaces:**
- Consumes: 现有 `GenericLobbyStateCapturePlan`、`GenericLobbyRuntimeIdentityCapturePlan`、`GenericLobbyOptionsCapturePlan` 和 `GenericLobbyCustomGameCapturePlan`。
- Produces: `GenericLobbyCapturePlan`，包含已有字段组 plan、generic metadata 可用性与 apply 模式。

- [ ] **Step 1: 写入失败的 snapshot-only 回归**

```cpp
const auto capture_plan = compose_generic_lobby_capture_plan(local_lobby, generic_metadata);
apply_generic_lobby_capture_plan(snapshot_only_lobby, capture_plan);
expect_shared_publish_count(0u);
```

Expected: 当前实现缺少统一 capture plan 或 snapshot-only 边界时，测试无法编译或 shared publish 计数断言失败。

- [ ] **Step 2: 实现纯 capture plan**

```cpp
struct GenericLobbyCapturePlan {
    GenericLobbyStateCapturePlan state;
    GenericLobbyRuntimeIdentityCapturePlan runtime_identity;
    GenericLobbyOptionsCapturePlan options;
    GenericLobbyCustomGameCapturePlan custom_game;
};

GenericLobbyCapturePlan compose_generic_lobby_capture_plan(
    const GBE_LocalLobby &current_lobby,
    const GenericLobbyMetadata &metadata);
```

Expected: compose 函数只读取参数和生成 plan；不访问 Store、不发送消息、不修改 `GBE_local_lobby`。

- [ ] **Step 3: 实现单次 Local apply**

```cpp
void apply_generic_lobby_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyCapturePlan &plan)
{
    apply_generic_lobby_state_capture_plan(lobby, plan.state);
    apply_generic_lobby_runtime_identity_capture_plan(lobby, plan.runtime_identity);
    apply_generic_lobby_options_capture_plan(lobby, plan.options);
    apply_generic_lobby_custom_game_capture_plan(lobby, plan.custom_game);
}
```

Expected: 字段组 apply 顺序保持 state、runtime identity、options、custom game；原有 stale-state 诊断继续由 coordinator 记录。

- [x] **Step 4a: 引入 payload snapshot capture facade**

`GBE_CaptureCurrentDotaLobbySnapshotForPayload()` 统一 cache、replay 与 details 的快照消费入口，并维持当前 shared restore、generic owner repair 与 Local apply 行为。

- [ ] **Step 4b: 迁移纯 snapshot projection 调用方**

将 `cache_template_replay`、`cache_payload`、replay snapshot 与 details update 改为纯 projection；仅构造并返回 snapshot，不执行 Local/shared/generic metadata 写入。先补齐 owner、member、LAN 与 custom runtime 回归。

- [ ] **Step 5: 运行 focused 与完整验证**

Run: `bash tools/run_gc_verification.sh --full`

Expected: 输出 `GC verification passed`，handler smoke 保持 response/push/publish 顺序。

### Task 3: 显式 Host Synchronization 边界

**Files:**
- Modify: `dll/gbe_dota_lobby_state_publish_coordinator.cpp`
- Modify: `dll/gbe_dota_lobby_launch_coordinator.cpp:455-644`
- Modify: `dll/gbe_dota_match_handlers.cpp:400`
- Modify: `dll/gbe_dota_chat_handlers.cpp:275`
- Test: `tools/gbe_dota_handler_test/smoke_test.cpp`
- Modify: `docs/gc/LOCAL_LOBBY_USAGE.md`

**Interfaces:**
- Consumes: `GenericLobbyCapturePlan` 与 `GBE_PublishSharedDotaLobbyState(const char *reason)`。
- Produces: coordinator 内部的显式 host sync helper，接收已应用的 Local lobby 和 reason。

- [ ] **Step 1: 写入 host capture publish 顺序 smoke**

```cpp
expect_event_sequence({
    "generic_capture_apply",
    "shared_publish",
    "details_response"
});
```

Expected: 当前路径的可观察顺序记录为基线，测试覆盖 host 与 client 两个角色。

- [ ] **Step 2: 实现窄同步 helper**

```cpp
bool Steam_Game_Coordinator::GBE_SyncCapturedDotaLobbyState(
    const char *reason,
    bool host_authoritative)
{
    if (!host_authoritative)
        return false;
    return GBE_PublishSharedDotaLobbyState(reason);
}
```

Expected: helper 不重新读取 metadata，不构造协议 payload；shared 写入继续经过 generation-gated publish 门面。

- [ ] **Step 3: 迁移一个 host capture 路径**

选择已具备 smoke 的 details update 或 7034 custom runtime refresh 路径，按原有 response/push 前后位置调用 helper。client 路径维持 observe/restore 行为。

- [ ] **Step 4: 验证 generation 与协议顺序**

Run: `bash tools/run_gc_verification.sh --full`

Expected: stale generation 拒绝保持；host 产生一个 shared publish；client 不产生 shared publish。

### Task 4: Source-aware Runtime Restore Plan

**Files:**
- Modify: `dll/gbe_dota_lobby_state.h`
- Modify: `dll/gbe_dota_lobby_state.cpp`
- Modify: `dll/gbe_dota_lobby_state_restore_coordinator.cpp:146-285`
- Test: `tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp`
- Test: `tools/gbe_dota_handler_test/smoke_test.cpp`
- Modify: `docs/gc/LOCAL_LOBBY_USAGE.md`

**Interfaces:**
- Consumes: `GBE_LocalLobby` 与 `GBE_SharedDotaLobbyState`。
- Produces: `SourceAwareSharedRuntimeRestorePlan`，包含 state、game_state、launch_phase、room_name、connect、match_id、server_id 和 game_start_time 的 apply 标志与诊断来源。

- [ ] **Step 1: 写入同 generation 覆盖回归**

```cpp
local.connect = "new-connect";
local.match_id = 22ull;
shared.connect = "old-connect";
shared.match_id = 11ull;
shared.generation = local.generation;
const auto plan = compose_source_aware_shared_runtime_restore_plan(local, shared);
expect_false(plan.apply_connect);
expect_false(plan.apply_match_id);
```

Expected: 当前 restore 行为在无来源判断时允许旧 shared 结果覆盖 Local，测试失败。

- [ ] **Step 2: 实现 restore plan 和 apply**

```cpp
struct SourceAwareSharedRuntimeRestorePlan {
    SharedLobbyRuntimeRestorePlan runtime;
    bool apply_connect{};
    std::string connect;
    bool apply_match_id{};
    std::uint64_t match_id{};
    RestoreSource source{};
};
```

Expected: plan 保留现有 READYUP、零值、空 connect 和 generation 规则；apply 函数不发布 shared Store。

- [ ] **Step 3: 迁移 client restore 聚合**

将 `GBE_RestoreSharedDotaLobbyState()` 的 runtime 字段组改为一次 compose/apply；保留 `changed` 聚合、client member 检查和 server adopt 顺序。

- [ ] **Step 4: 完整验证**

Run: `git diff --check && bash tools/run_gc_verification.sh --full`

Expected: 输出 `GC verification passed`；测试覆盖 READYUP、RUN、postgame 和 custom-game runtime。

### Task 5: 迁移结案与后续规格切分

**Files:**
- Modify: `docs/gc/CURRENT.md`
- Modify: `docs/gc/ACTIVE_QUEUE.md`
- Modify: `docs/gc/LOCAL_LOBBY_USAGE.md`
- Modify: `docs/gc/PHASE_D_BOUNDARY.md`
- Modify: `.monkeycode/specs/gc-launch-runtime-single-write/requirements.md`
- Modify: `.monkeycode/specs/gc-launch-runtime-single-write/design.md`

**Interfaces:**
- Consumes: Tasks 1-4 的验证结果和 writer inventory。
- Produces: 已更新的字段来源表与后续独立规格入口。

- [ ] **Step 1: 记录每个已迁移字段组**

为 generic capture、host sync 与 shared restore 分别记录 Local apply、shared publish、client restore、host authority 和测试入口。

- [ ] **Step 2: 创建后续债务规格目录**

建立独立规格用于 generic metadata publish、8052 lifecycle pre-write 和 chat postgame tombstone；每个规格只覆盖一个 state 或 side-effect 边界。

- [ ] **Step 3: 最终验证检查点**

Run: `git diff --check && bash tools/run_gc_verification.sh --full`

Expected: 输出 `GC verification passed`。保留变更集供人工审核和提交批准。

## Self-Review

- 覆盖：R5 对应 Tasks 2-3；R6 对应 Task 4；R7 对应 Tasks 1-5；既有 R1-R4 由所有任务的 pure plan、顺序 smoke 与完整验证维持。
- 占位符：计划包含实际接口、测试代码和验证命令；每项任务都可独立审查。
- 类型一致性：Task 2 产出 `GenericLobbyCapturePlan`；Task 3 消费该计划；Task 4 独立产出 `SourceAwareSharedRuntimeRestorePlan`，避免与既有 `SharedLobbyRuntimeRestorePlan` 混用。
