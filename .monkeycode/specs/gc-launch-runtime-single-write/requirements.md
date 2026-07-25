# GC Launch Runtime 单写入需求

## 目标

本切片收敛 Dota GC lobby launch/runtime 字段的生产写入路径，降低 `GBE_local_lobby` 与 shared Store 双轨状态的维护风险。切片保留 `GBE_local_lobby` 作为 GC 运行时工作副本，并保持 host-only shared publish、client 观察恢复、generation gate、deferred task gate 和协议副作用顺序。

## 术语

- **Launch/runtime 字段**：`state`、`game_state`、`launch_phase` 与 `launch_*` 标志字段。
- **Local 工作副本**：`GBE_local_lobby` 中供当前 GC handler 与 coordinator 使用的 Lobby 状态。
- **Shared 快照**：由 `gbe::dota_lobby_state::Store` 保存的跨 client/server GC 状态。
- **单写入入口**：由 plan、action 或专用 apply 边界执行 launch/runtime 字段更新的路径。
- **Host publish**：host 权威 GC 经 generation-gated Store API 发布 shared 快照的过程。

## 需求

### R1 Launch/runtime 写入边界

**用户故事：** 作为 Dota GC 维护者，我希望 launch/runtime 字段经明确写入边界更新，以便定位状态变化的来源和顺序。

#### 验收标准

1. WHEN 生产路径更新 launch/runtime 字段，系统 SHALL 通过专用 plan/apply 或 lifecycle action/executor 边界执行更新。
2. WHEN 同一协议处理路径需要更新多个 launch/runtime 字段，系统 SHALL 以一次边界调用应用该字段组。
3. WHEN 生产路径新增 launch/runtime 写入，系统 SHALL 在 `LOCAL_LOBBY_USAGE.md` 中记录字段所有权和写入边界。

### R2 Shared publish 角色语义

**用户故事：** 作为 Dota GC 维护者，我希望本切片保持现有角色权威语义，以便协议响应和跨 GC 行为保持稳定。

#### 验收标准

1. WHILE 当前 GC 是 host 权威路径，系统 SHALL 经 generation-gated Store publish 门面发布更新后的 launch/runtime 快照。
2. WHILE 当前 GC 是 client 观察路径，系统 SHALL 保持现有 shared snapshot 恢复语义。
3. WHEN shared snapshot generation 早于 Local 工作副本 generation，系统 SHALL 保持现有 generation gate 对陈旧状态的拒绝结果。

### R3 协议与异步顺序

**用户故事：** 作为 Dota GC 维护者，我希望状态边界收敛保留既有协议和异步顺序，以便客户端可观察行为稳定。

#### 验收标准

1. WHEN launch/runtime 写入发生在有 response 或 push 的处理路径，系统 SHALL 保持现有 action、response、push 和 publish 的相对顺序。
2. WHEN launch/runtime 写入触发 deferred work，系统 SHALL 保持 deferred slot 的 lobby id 和 generation 验证。
3. IF apply 边界拒绝 transition，系统 SHALL 保持当前 Local 工作副本、Shared 快照和 outbound message 序列。

### R4 回归护栏

**用户故事：** 作为 Dota GC 维护者，我希望每次迁移都有可重复验证，以便识别状态语义或协议顺序回归。

#### 验收标准

1. WHEN 实现任一 launch/runtime 写入迁移，系统 SHALL 增加或更新 focused flow、state-machine 或 handler smoke 回归。
2. WHEN 切片完成，系统 SHALL 通过 `bash tools/run_gc_verification.sh --full`。
3. WHEN 切片完成，系统 SHALL 通过 `git diff --check`。

## 范围

- 包含：`state`、`game_state`、`launch_phase` 和 `launch_*` 字段的生产写入边界。
- 包含：host-only shared publish 与 client restore 的现有语义保持。
- 排除：members、chat、broadcast、owner_hero_id、server_id、connect、match_id 的所有权变更。
- 排除：生产 CompositionRoot、Coordinator 大拆分和真协议 L2/L4 验证恢复。
