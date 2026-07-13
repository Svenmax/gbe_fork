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
| `LegacyLifecycleActionsRequested` | **Legacy 门闩** | custom_game 7070/8052/8053：accept 后跑 `compute_custom_game_*` + `GBE_ExecuteDotaCustomGameLifecycleTransition` |
| `LegacyTeardownActionsRequested` | **Legacy 门闩** | leave / abandon / signout / postgame finalize：accept 后跑各自 Queue / action_list / cleanup |

目标（Phase C）：把 `Legacy*` 逐步换成 **具名 Effect + 统一 action builder**，handler 只做 parse → SM → Execute。

## 3. LegacyLifecycleActionsRequested 调用面

入口 API：`transition_custom_game_request` → `accepted_custom_game_request` **总是**追加该 effect。

| 文件 | emsg / 事件 | SM 后真实副作用 |
|------|-------------|-----------------|
| `gbe_dota_custom_game_lifecycle_handlers.cpp` | 7070 ReadyUp (`EventKind::Run`) | `compute_custom_game_ready_up_transition` → `GBE_ExecuteDotaCustomGameLifecycleTransition` |
| 同上 | 8052 StartedLoading (`Loading`) | `compute_custom_game_started_loading_transition` → Execute |
| 同上 | 8053 FinishedLoading (`Loaded`) | `compute_custom_game_finished_loading_transition` → Execute（含 member runtime 标志） |

协调器：`GBE_ExecuteDotaCustomGameLifecycleTransition`（`custom_game_lifecycle_coordinator.cpp`）内部再 `build_transition_actions` + `GBE_ExecuteDotaLifecycleActions`。

**收敛含义：** 将 `LaunchLifecycleTransitionDecision` 的 apply/publish/runtime 语义编码进 SM EffectList（或 `TransitionEffects`），删除 “Legacy 门闩 + 第二套 compute_*” 双决策。

## 4. LegacyTeardownActionsRequested 调用面

入口 API：`transition_teardown`（Leave / Abandon / PostGame / Reset + Initiate|Finalize）。

| 文件 | 场景 | Stage | SM 后真实副作用 |
|------|------|-------|-----------------|
| `lobby_lifecycle_handlers.cpp` | 7035 Abandon | Initiate | discard launch / suppress / `GBE_QueueDotaPostGameTeardown` |
| 同上 | 7004 SignOut | Initiate | `QueueDotaPostGameTeardown` + details + 25 via action_list |
| 同上 | 7040 Leave | Initiate | leave action_list / cache unsub 等（handler 后续） |
| `lobby_flow_coordinator.cpp` | abandon finalize | Finalize | `abandon_finalize_action_list` → Execute |
| 同上 | normal signout finalize | Finalize | signout cleanup action_list → Execute |
| `lobby_list_handlers.cpp` | list 路径 leave teardown | Finalize | leave 相关 teardown 门闩 |
| `lobby_state_member_coordinator.cpp` | player postgame cleanup | Finalize | `player_postgame_cleanup_action_list` → Execute |

**收敛含义：** 为 Initiate/Finalize × (Leave|Abandon|PostGame|Reset) 定义 **具名 teardown effect**（如 `PostGameTeardownQueued`、`CacheUnsubscribedRequested`、`RuntimeCleared`），由 action builder 展开；SM 应推进 lifecycle State（今日 `transition_teardown` **不改** `state.lifecycle`，只发 Legacy effect）。

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
| P0 | Teardown：7040 Leave Initiate | 路径清晰、replay 有 `lobby_leave_teardown` | 不改消息序 |
| P0 | Teardown：7035 Abandon Initiate | smoke 覆盖 abandon 队列 | 不改 suppress/discard 语义 |
| P1 | Teardown：7004 SignOut Initiate | 与 25/7010 序强相关 | 需 recorder 序测 |
| P1 | Teardown Finalize 三处（flow / member / list） | 对称 Initiate | 区分 host/player/arcade |
| P2 | Custom game 7070/8052/8053 | 双决策（SM + compute_*）最重 | 保持 direct/wrapped 等价测 |
| P3 | 统一 inventory/network 的 member runtime 必须经 SM | 消除旁路 | 不改 hero 权威 API |

每步完成定义：对应 Legacy effect **在该路径不再出现**（或仅 debug 别名），`verification --full` 绿，相关 smoke/replay 仍绿。

## 7. 与 CURRENT 风险的关系

- CURRENT 风险 #4「lifecycle SM 半接入」= 本文 §3–§4 的 Legacy 门闩面。
- 本盘点 **不** 声明 Phase C 完成；完成以 Legacy 路径归零 + 测试为准。

## 8. 维护

- 新增 `transition_*` 或 `contains(Legacy*)` → 更新 §3/§4 表。
- 新增具名 EffectKind → 更新 §2，并从 Legacy 表删除对应行。
