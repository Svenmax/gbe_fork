# Generic Metadata Publish 后续需求

## 目标

本切片为 generic lobby metadata 的 shared publish 决策建立独立、可审计的边界。当前 `GBE_CaptureCurrentDotaLobbyState()` 已将 metadata 组合为 `GenericLobbyCapturePlan` 并应用到 Local 工作副本；后续实现负责定义何时由 host 将已应用的 Local 快照同步到 shared Store。

## 术语

- **Generic metadata**：generic lobby 中的 state、runtime identity、options 与 custom_game 字段。
- **Local apply**：`apply_generic_lobby_capture_plan()` 对 `GBE_local_lobby` 或 payload projection 的单次字段组更新。
- **Host sync**：`GBE_SyncCapturedDotaLobbyState()` 经 `GBE_PublishSharedDotaLobbyState()` 写入 shared Store 的显式动作。
- **Snapshot projection**：`GBE_DotaLobbyCaptureMode::PureSnapshotProjection` 下仅构建 payload 快照的读取路径。

## 需求

### R1 Publish 意图

**用户故事：** 作为 Dota GC 维护者，我希望 generic metadata 的 shared publish 具有显式意图，以便 Local capture 与跨 GC 同步的触发条件可审计。

#### 验收标准

1. 当 host 权威调用路径完成 generic metadata 的 Local apply 且需要跨 GC 可见状态时，系统应通过单一 host sync 边界发布已更新的 Local 快照。
2. 当 client 观察路径完成 generic metadata capture 时，系统应保持现有 shared restore 与角色权威规则。
3. 当 generic metadata capture 只服务于 payload、cache 或 replay projection 时，系统应保留 Local 工作副本、shared Store 与 owner repair/adopt 的当前结果。

### R2 顺序与 generation

**用户故事：** 作为 Dota GC 维护者，我希望 publish 边界保留生命周期与 generation 语义，以便协议消息和异步状态稳定。

#### 验收标准

1. 当 host sync 被接纳时，系统应经 `GBE_PublishSharedDotaLobbyState()` 的 generation-gated Store 更新路径发布快照。
2. 当 Store 返回陈旧 generation 结果时，系统应保留 Local 工作副本并保留现有诊断行为。
3. 当 capture 路径伴随 response、push 或 deferred work 时，系统应保持既有 action、outbound message 与 publish 的相对顺序。

### R3 验证护栏

**用户故事：** 作为 Dota GC 维护者，我希望该切片有可重复的验证条件，以便发现双轨收敛回归。

#### 验收标准

1. 当实现 host sync 决策时，系统应增加 focused flow 或 handler smoke 覆盖 host publish、client observe 与 pure projection。
2. 当实现完成时，变更集应通过 `bash tools/run_gc_verification.sh --full`。
3. 当实现完成时，变更集应通过 `git diff --check`。

## 现有依据

- `dll/gbe_dota_lobby_state_publish_coordinator.cpp:76`：`GBE_CaptureCurrentDotaLobbyState()` 选择同步或纯 projection capture。
- `dll/gbe_dota_lobby_state_publish_coordinator.cpp:152`：generic metadata 组成 `GenericLobbyCapturePlan`。
- `dll/gbe_dota_lobby_state_publish_coordinator.cpp:300`：`GBE_SyncCapturedDotaLobbyState()` 是现有 host sync 入口。
- `dll/gbe_dota_lobby_state_publish_coordinator.cpp:355`：`GBE_PublishSharedDotaLobbyState()` 使用 generation-gated Store 更新。
- `dll/gbe_dota_lobby_state.cpp:573` 与 `dll/gbe_dota_lobby_state.cpp:617`：generic capture plan 的 compose/apply 实现。

## 范围外

- members、chat、broadcast 与 `owner_hero_id` 的字段所有权迁移。
- `Steam_Game_Coordinator` 的 CompositionRoot 改造或横向拆分。
- 真协议 L2/L4 验收与 Store generation 语义调整。
