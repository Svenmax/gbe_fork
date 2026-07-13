# Legacy lifecycle effect 盘点（C2）

> 对照代码日期：2026-07-13。**不改运行时语义**；只冻结 SM 半接入现状与迁移顺序。
> 源：`dll/gbe_dota_lifecycle_state_machine.h` + 下列 call site。

## 1. 模型一句话

Lifecycle SM 已能 **分类事件 / generation / 部分 State**，但多数生产路径在 `accepted() && effects.contains(Legacy*)` 后，仍执行 **handler 内既有命令式逻辑**（`compute_*_transition`、`Queue*`、`build_*_action_list`）。  
`Legacy*` effect 的角色是 **门闩（gate）**，不是完整 effect 载荷。

## 2. EffectKind 分类

| EffectKind | 角色 | 生产消费方式 |
|------------|------|----------------|
| `StateChanged` | SM 状态变化记录 | custom_game 接受路径可伴随；少见独立执行 |
| `GenerationAdvanced` | generation 推进 | 核心 `transition()` 路径 |
| `ReconnectQueued` | reconnect 门闩 | 核心 `transition()` |
| `PracticeLobbyDetailsRequested` | **具名** runtime effect | match 7034 poll → `build_transition_actions` → `GBE_ExecuteDotaLifecycleActions` |
| `RuntimeMemberUpdateRequested` | **具名** | match / inventory → `build_member_runtime_actions` → Execute |
| `RuntimeGameStateUpdateRequested` | **具名** | match → `build_transition_actions` → Execute |
| `CustomGameLifecycleActionsRequested` | **具名 custom-game 门闩（C5/C7）** | SM 门闩仍发此 token；C7 起由 `decide_*` 统一 SM+compute_*，handler 只 Execute |
| `TeardownLeave* / Abandon* / PostGame* / Reset*` | **具名 teardown 路径门闩（C6）** | 按 event+stage 分发；`TeardownActionsRequested` 已删除；副作用经 action_list / QueuePostGame |

目标（Phase C）：把 `Legacy*` 逐步换成 **具名 Effect + 统一 action builder**，handler 只做 parse → SM → Execute。

## 3. CustomGameLifecycleActionsRequested 调用面（原 LegacyLifecycle）

入口 API：`transition_custom_game_request` → `accepted_custom_game_request` **总是**追加该 effect。
C7：handler 只调 `decide_*`；SM 门闩 + `compute_*` 在 coordinator 侧统一。

| 文件 | emsg / 事件 | 决策入口 | SM 后真实副作用 |
|------|-------------|---------|-----------------|
| `custom_game_lifecycle_handlers.cpp` | 7070 ReadyUp | `decide_ready_up` | Execute（仅当 apply_lobby_state） |
| 同上 | 8052 StartedLoading | `decide_started_loading` | Execute |
| 同上 | 8053 FinishedLoading | `decide_finished_loading` | Execute（含 member runtime 标志） |
| `custom_game_lifecycle_coordinator.cpp` | （decide 内部） | SM gate + `compute_custom_game_*` | `build_transition_actions` + Execute |

**收敛含义：** C7 已去掉 handler 内双决策；后续可将 `LaunchLifecycleTransitionDecision` 语义进一步编码进 SM EffectList。

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

**下一步：** QueuePostGame 状态突变（chat channel / publish）进一步 action 化。

## 5. 已具名（对照，非 Legacy）

| 路径 | SM API | Effect | 执行 |
|------|--------|--------|------|
| 7034 runtime member / hero | `transition_runtime_member` | `RuntimeMemberUpdateRequested` | `build_member_runtime_actions` |
| 7034 game_state 推进 | `transition_runtime_game_state` | `RuntimeGameStateUpdateRequested` | `build_transition_actions` |
| 7034 poll details | `transition_runtime_poll` | `PracticeLobbyDetailsRequested` | `build_transition_actions` |
| inventory equip → server hero | （直接）`build_member_runtime_actions` | 常不经 SM | Execute（与 SM 并行入口） |

注意：inventory / network_callbacks 可 **绕过 SM** 直接 `build_member_runtime_actions`；迁移时要统一 “是否必须先 SM”。

## 6. 建议迁移顺序（只排序，本轮不实施）

| 优先级 | 项 | 理由 | 停手 |
|--------|----|------|------|
| P0 | ~~Teardown 门闩具名化~~ | **C3+C4 完成** | — |
| P1 | ~~Teardown 路径门闩 + action builder~~ | **C6 完成**：event+stage 分发；7035 preflight / list leave 进 action_list | — |
| P2 | ~~Custom game 单一 decide_*~~ | **C7 完成**：handler 只 parse→decide→Execute | — |
| P3 | 统一 inventory/network 的 member runtime 必须经 SM | 消除旁路 | 不改 hero 权威 API |
| P4 | QueuePostGame 状态突变 action 化 | chat/publish 仍在 coordinator | 不改消息序 |

每步完成定义：对应 Legacy effect **在该路径不再出现**（或仅 debug 别名），`verification --full` 绿，相关 smoke/replay 仍绿。

## 7. 与 CURRENT 风险的关系

- CURRENT 风险 #4「lifecycle SM 半接入」= 本文 §3–§4 的 Legacy 门闩面。
- 本盘点 **不** 声明 Phase C 完成；完成以 Legacy 路径归零 + 测试为准。

## 8. 维护

- 新增 `transition_*` 或 `contains(Legacy*)` → 更新 §3/§4 表。
- 新增具名 EffectKind → 更新 §2，并从 Legacy 表删除对应行。
