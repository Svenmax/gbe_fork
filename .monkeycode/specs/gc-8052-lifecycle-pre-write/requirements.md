# 8052 Lifecycle Pre-write 后续需求

## 目标

本切片为 custom-game 8052 started-loading 生命周期中的 pre-write 边界建立独立规格。当前路径由纯 transition decision 产生 `LobbyStateApply`、`LaunchPhaseMark`、`RuntimeLobbyDetailsUpdate` 与 fallback `SharedLobbyPublish` action；后续实现将明确协议接收前后各项 Local 字段写入的归属和顺序。

## 术语

- **8052**：custom-game started-loading 协议消息。
- **Pre-write**：在 deferred runtime update 或 outbound 副作用前对 Local lifecycle 字段执行的更新边界。
- **Fallback publish**：runtime update 未入队时执行的 `SharedLobbyPublish` action。
- **Action list**：`GBE_DotaActionList` 中决定 Local apply、launch mark、runtime update 与 publish 顺序的序列。

## 需求

### R1 8052 写入归属

**用户故事：** 作为 Dota GC 维护者，我希望 8052 的 pre-write 有唯一归属，以便 RUN transition 的 Local 状态可预测。

#### 验收标准

1. 当 8052 gate 与 lobby 匹配条件接纳 started-loading transition 时，系统应通过 action list 中的明确 apply 边界更新 lifecycle 字段组。
2. 当 8052 gate 拒绝 transition 或 lobby 条件失配时，系统应保留现有 Local state、launch phase 与 outbound sequence。
3. 当 runtime update 入队时，系统应保留 `LobbyStateApply`、`LaunchPhaseMark` 与 `RuntimeLobbyDetailsUpdate` 的既有相对顺序。

### R2 Fallback 与异步 gate

**用户故事：** 作为 Dota GC 维护者，我希望 8052 fallback 保留队列与 publish 语义，以便 delayed runtime work 和 shared 状态收敛。

#### 验收标准

1. 当 runtime update 成功入队时，系统应跳过带 `only_when_runtime_update_not_queued` 条件的 fallback publish。
2. 当 runtime update 未入队时，系统应执行既有 fallback shared publish 与 details update 行为。
3. 当 deferred runtime work 执行时，系统应继续验证 lobby id 与 generation。

### R3 验证护栏

**用户故事：** 作为 Dota GC 维护者，我希望 8052 pre-write 迁移具备顺序回归测试，以便识别协议可观察变化。

#### 验收标准

1. 当实现 pre-write 边界时，系统应增加覆盖 direct 与 wrapped 8052 的 action sequence 等价性测试。
2. 当实现完成时，变更集应通过 `bash tools/run_gc_verification.sh --full`。
3. 当实现完成时，变更集应通过 `git diff --check`。

## 现有依据

- `dll/gbe_dota_custom_game_lifecycle_handlers.cpp:88`：8052 请求解析并调用 started-loading decision。
- `dll/gbe_dota_custom_game_lifecycle_coordinator.cpp:350`：`decide_started_loading()` 生成生命周期 decision。
- `dll/gbe_dota_lobby_state.cpp:1050`：`compute_custom_game_started_loading_transition()` 决定 RUN advance、queue 与 fallback。
- `dll/gbe_dota_lifecycle_actions.cpp:41`：transition 生成 runtime update action。
- `dll/gbe_dota_custom_game_lifecycle_coordinator.cpp:94`：executor 应用 action list 并处理 fallback 条件。
- `dll/gbe_dota_lobby_launch_coordinator.cpp:194`：`GBE_TryQueueDotaRuntimeLobbyDetailsUpdate()` 是 deferred runtime update 入口。

## 范围外

- 7041 launch setup、8053 finished-loading 与非 custom-game 生命周期迁移。
- 生命周期 state machine 的 gate token 模型替换。
- protocol wire 格式、direct/wrapped adapter 协议与真协议 L2/L4 验收。
