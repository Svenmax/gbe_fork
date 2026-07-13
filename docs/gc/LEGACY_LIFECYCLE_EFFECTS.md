# Lifecycle effect 盘点（C2 起；C-exit 收口）

> 对照代码日期：2026-07-13。**不改运行时语义**。
> 源：`dll/gbe_dota_lifecycle_state_machine.h` + 下列 call site。
> **C-exit：** 生产 `EffectKind` 已无 `Legacy*` 符号；P0–P4 迁移项全部勾完。

## 1. 模型一句话

Lifecycle SM 负责 **分类事件 / generation / 部分 State**，并产出 **具名门闩 Effect**。
生产路径形态：`accepted() && effects.contains(NamedGate)` → `decide_*` / `*_action_list` → `GBE_ExecuteDotaLifecycleActions`。
门闩 token **不是** 完整 effect 载荷；载荷在 pure planner / action_list 中构建。

## 2. EffectKind 分类

| EffectKind | 角色 | 生产消费方式 |
|------------|------|----------------|
| `StateChanged` | SM 状态变化记录 | custom_game 接受路径可伴随；少见独立执行 |
| `GenerationAdvanced` | generation 推进 | 核心 `transition()` 路径 |
| `ReconnectQueued` | reconnect 门闩 | 核心 `transition()` |
| `PracticeLobbyDetailsRequested` | **具名** runtime effect | match 7034 poll → `build_transition_actions` → Execute |
| `RuntimeMemberUpdateRequested` | **具名** | `decide_member_runtime_actions` → Execute |
| `RuntimeGameStateUpdateRequested` | **具名** | match → `build_transition_actions` → Execute |
| `CustomGameLifecycleActionsRequested` | **具名 custom-game 门闩（C5/C7）** | `decide_*` 统一 SM+compute_*，handler 只 Execute |
| `TeardownLeave* / Abandon* / PostGame* / Reset*` | **具名 teardown 路径门闩（C6）** | 按 event+stage 分发；副作用经 action_list / QueuePostGame |

**已删除：** `LegacyTeardownActionsRequested`、`LegacyLifecycleActionsRequested`、以及任何 `Legacy*` EffectKind。

## 3. CustomGameLifecycleActionsRequested 调用面

入口 API：`transition_custom_game_request` → accept 时追加该 effect。
C7：handler 只调 `decide_*`；SM 门闩 + `compute_*` 在 coordinator 侧统一。

| 文件 | emsg / 事件 | 决策入口 | SM 后真实副作用 |
|------|-------------|---------|-----------------|
| `custom_game_lifecycle_handlers.cpp` | 7070 ReadyUp | `decide_ready_up` | Execute（仅当 apply_lobby_state） |
| 同上 | 8052 StartedLoading | `decide_started_loading` | Execute |
| 同上 | 8053 FinishedLoading | `decide_finished_loading` | Execute（含 member runtime 标志） |
| `custom_game_lifecycle_coordinator.cpp` | （decide 内部） | SM gate + `compute_custom_game_*` | `build_transition_actions` + Execute |

## 4. Teardown 门闩调用面

入口 API：`transition_teardown`（Leave / Abandon / PostGame / Reset + Initiate|Finalize）。
C6：accept 时按 kind+stage 发路径门闩（`teardown_effect_for`）。

| 文件 | 场景 | Stage | 门闩 token | SM 后真实副作用 |
|------|------|-------|------------|-----------------|
| `lobby_lifecycle_handlers.cpp` | 7035 Abandon | Initiate | TeardownAbandonInitiateRequested | `abandon_initiate_preflight_action_list` + QueuePostGame |
| 同上 | 7004 SignOut | Initiate | TeardownPostGameInitiateRequested | QueuePostGame + details + 25 action_list |
| 同上 | 7040 Leave | Initiate | TeardownLeaveInitiateRequested | `leave_lobby_cache_unsubscribed_action_list` |
| `lobby_flow_coordinator.cpp` | abandon finalize | Finalize | TeardownAbandonFinalizeRequested | `abandon_finalize_action_list` |
| 同上 | normal signout finalize | Finalize | TeardownPostGameFinalizeRequested | `normal_signout_finalize_action_list` |
| `lobby_list_handlers.cpp` | list leave teardown | Finalize | TeardownLeaveFinalizeRequested | `leave_lobby_finalize_action_list` |
| `lobby_state_member_coordinator.cpp` | player postgame | Initiate | TeardownPostGameInitiateRequested | `player_postgame_cleanup_action_list` |
| `lobby_launch_coordinator.cpp` | QueuePostGame | Initiate 载荷 | （payload builder） | C9：`postgame_teardown_action_list` → Execute |

## 5. Runtime 具名路径（对照）

| 路径 | 决策入口 | Effect | 执行 |
|------|----------|--------|------|
| 7034 connected/disconnected / restore hero | `decide_member_runtime_actions` | `RuntimeMemberUpdateRequested`（内部） | Execute |
| inventory 2569 equip → server hero | `decide_member_runtime_actions` | 同上 | Execute |
| network inventory_response remote hero | `decide_member_runtime_actions` | 同上 | Execute |
| 7034 game_state 推进 | `transition_runtime_game_state` | `RuntimeGameStateUpdateRequested` | `build_transition_actions` |
| 7034 poll details | `transition_runtime_poll` | `PracticeLobbyDetailsRequested` | `build_transition_actions` |

C8：生产路径禁止直接 `build_member_runtime_actions`；测试/纯 builder 仍可直调。

## 6. 迁移完成表（C-exit）

| 优先级 | 项 | 状态 |
|--------|----|------|
| P0 | Teardown 门闩具名化 | **C3+C4 完成** |
| P1 | Teardown 路径门闩 + action builder | **C6 完成** |
| P2 | Custom game 单一 decide_* | **C7 完成** |
| P3 | member runtime 经 SM | **C8 完成** |
| P4 | QueuePostGame 状态突变 action 化 | **C9 完成** |

**C-exit 复核（2026-07-13）：**
- `dll/` 内 `EffectKind` 无 `Legacy*` 枚举成员
- 生产 call site 均检查具名 token 或走 `decide_*` / `*_action_list`
- `verification --full` 为 C-exit 闸口（与代码改动无关时仍要求绿）

## 7. 与 CURRENT 风险的关系

- 原「lifecycle SM 半接入 / Legacy 门闩」风险：C3–C9 后 **已收敛**（具名门闩 + action 化）。
- **剩余架构债**（不阻塞 C-exit 勾选）：门闩仍是 gate token 而非完整 SM 载荷；`Steam_Game_Coordinator` 上帝类与 local/shared 双轨见 CURRENT 风险 #2/#3（Phase D 范围）。
- Phase C 代码 KPI「Legacy effect 收敛」：以本表 §6 全勾 + 无 `Legacy*` EffectKind 为准。

## 8. 维护

- 新增 `transition_*` 或具名 EffectKind → 更新 §2–§5。
- **禁止** 再引入 `Legacy*` EffectKind；新路径必须具名 token + action_list / decide_*。
