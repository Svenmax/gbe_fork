# Postgame Chat Tombstone 后续需求

## 目标

本切片为 postgame 期间旧 chat channel 的 tombstone 生命周期建立独立规格。当前 postgame action 一次应用 state、chat 与 cache 清理字段组，并保留 pre-postgame channel id；shared Store 已用 generation tombstone 保护 runtime clear。后续实现将定义旧 chat channel 的抑制、消费和清理边界。

## 术语

- **Pre-postgame channel**：进入 postgame 前由 `abandon_pre_postgame_chat_channel_id` 保存的 chat channel id。
- **Chat tombstone**：供后续 chat payload 判定旧 channel 已关闭或已消费的具名状态。
- **Postgame apply**：`PostGameLobbyStateApply` action 通过 `apply_postgame_lobby_state_plan()` 一次应用 state、chat 与 cache 字段组。
- **Store tombstone**：`Store::compare_clear()` 为当前 generation 保存的空 shared snapshot。

## 需求

### R1 Tombstone 建立

**用户故事：** 作为 Dota GC 维护者，我希望 postgame 旧 chat channel 具有显式 tombstone，以便旧 channel payload 的处理范围可验证。

#### 验收标准

1. 当 postgame teardown 接纳 `suppress_previous_chat_channel` 时，系统应在 postgame apply 边界记录对应的 pre-postgame channel id 与 tombstone 状态。
2. 当 postgame teardown 未接纳旧 channel 抑制时，系统应保留当前 chat channel 语义。
3. 当新的 postgame channel 已建立时，系统应保持 postgame chat id、name、type 与 cache 清理字段组的现有 apply 顺序。

### R2 消费与清理

**用户故事：** 作为 Dota GC 维护者，我希望旧 chat tombstone 的消费与 runtime clear 按 generation 受保护，以便延迟 payload 无法影响新 lobby。

#### 验收标准

1. 当收到属于 tombstone channel 的旧 chat payload 时，系统应按显式消费规则处理该 payload。
2. 当 postgame 清理完成时，系统应只清理当前 lobby generation 对应的 tombstone 状态。
3. 当新 generation 产生新的 lobby 时，系统应保留新 lobby 的 chat 状态和 shared Store snapshot。

### R3 验证护栏

**用户故事：** 作为 Dota GC 维护者，我希望 tombstone 迁移具备协议和 generation 回归，以便识别跨 lobby 污染。

#### 验收标准

1. 当实现 chat tombstone 时，系统应增加 postgame teardown、旧 channel payload、new generation 与 runtime clear 的 focused 回归。
2. 当实现完成时，变更集应通过 `bash tools/run_gc_verification.sh --full`。
3. 当实现完成时，变更集应通过 `git diff --check`。

## 现有依据

- `dll/gbe_dota_lobby_launch_coordinator.cpp:550`：`GBE_QueueDotaPostGameTeardown()` 捕获 pre-postgame channel 并执行 postgame action list。
- `dll/gbe_dota_lobby_flow.cpp:250`：`postgame_teardown_action_list()` 构建 `PostGameLobbyStateApply` action。
- `dll/gbe_dota_custom_game_lifecycle_coordinator.cpp:101`：executor 将 action 转换为 `PostGameLobbyStateApplyPlan`。
- `dll/gbe_dota_lobby_state.cpp:1006`：`apply_postgame_lobby_state_plan()` 一次更新 state、chat 和 cache 字段组。
- `dll/gbe_dota_chat_handlers.cpp:190`：chat handler 使用 `abandon_pre_postgame_chat_channel_id`。
- `dll/steam_game_coordinator.cpp:1047`：runtime clear 通过 `Store::compare_clear(generation)` 建立 generation tombstone。
- `dll/gbe_dota_lobby_state_store.cpp:60`：`Store::compare_clear()` 保存当前 generation 的空 snapshot。

## 范围外

- 普通 leave、broadcast 和 lobby member chat 的协议重构。
- shared Store 的 tombstone 数据模型或 generation 比较规则变更。
- postgame 的 persona、cache payload wire 格式与真协议 L2/L4 验收。
