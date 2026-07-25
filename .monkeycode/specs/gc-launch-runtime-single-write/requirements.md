# GC Launch Runtime 单写入需求

## 目标

本规格分阶段收敛 Dota GC lobby 的 Local/shared 双轨状态边界。第一阶段保留 `GBE_local_lobby` 作为 GC 运行时工作副本，并将 generic capture、shared restore 与 shared publish 的字段来源、应用方式和同步时机显式化。各阶段保持 host-only shared publish、client 观察恢复、generation gate、deferred task gate 和协议副作用顺序。

## 术语

- **Launch/runtime 字段**：`state`、`game_state`、`launch_phase` 与 `launch_*` 标志字段。
- **Local 工作副本**：`GBE_local_lobby` 中供当前 GC handler 与 coordinator 使用的 Lobby 状态。
- **Shared 快照**：由 `gbe::dota_lobby_state::Store` 保存的跨 client/server GC 状态。
- **单写入入口**：由 plan、action 或专用 apply 边界执行 launch/runtime 字段更新的路径。
- **Host publish**：host 权威 GC 经 generation-gated Store API 发布 shared 快照的过程。
- **字段来源**：产生某字段候选值的路径，分为 generic capture、host publish 与 client observed shared restore。
- **同步边界**：决定 Local 工作副本是否需要产生 shared Store 更新的显式 action 或门面调用点。
- **双轨收敛**：为字段组定义 Local、shared 快照与 restore 的所有权规则，并通过单一同步边界保持收敛。

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

### R5 双轨字段来源与同步边界

**用户故事：** 作为 Dota GC 维护者，我希望 generic capture、Local 工作副本和 shared 快照具有明确的同步边界，以便任一字段更新的来源与传播路径可验证。

#### 验收标准

1. WHEN generic lobby metadata 产生 Local 字段候选值，系统 SHALL 通过纯 capture plan 表达字段值和来源。
2. WHEN 调用路径需要把 generic capture 结果传播到 shared Store，系统 SHALL 在显式 host 同步边界经 `GBE_PublishSharedDotaLobbyState()` 发布更新后的 Local 快照。
3. WHEN 调用路径只需要构建 details、cache 或 replay snapshot，系统 SHALL 使用不产生 shared Store 更新的 snapshot 路径。
4. WHILE 当前 GC 是 client 观察路径，系统 SHALL 仅应用 shared restore 允许的字段组。
5. WHEN Local runtime identity 与同 generation shared 快照发生竞争，系统 SHALL 按字段组权威规则保留已定义的赢家并记录诊断来源。

### R6 Restore 权威规则

**用户故事：** 作为 Dota GC 维护者，我希望 shared restore 具备字段组权威规则，以便旧 shared 快照不会覆盖同 generation 的较新 Local runtime 状态。

#### 验收标准

1. WHEN shared restore 处理 runtime identity 或 launch state，系统 SHALL 使用包含字段写许可和来源的 restore plan。
2. WHILE shared snapshot 通过 host publish 产生，系统 SHALL 保持现有 client observe 和 server adopt 角色语义。
3. IF restore plan 拒绝字段覆盖，系统 SHALL 保持 Local 工作副本、shared 快照和 outbound message 序列。
4. WHEN restore plan 接纳字段覆盖，系统 SHALL 保持 READYUP、RUN、postgame 与 custom-game 已有回退保护。

### R7 收敛观测与阶段控制

**用户故事：** 作为 Dota GC 维护者，我希望每个双轨收敛切片具备可观察的完成条件，以便部署前定位状态分歧。

#### 验收标准

1. WHEN 实现字段组收敛切片，系统 SHALL 在 `LOCAL_LOBBY_USAGE.md` 记录字段来源、Local apply 边界、shared publish 边界与 restore 规则。
2. WHEN 切片涉及 host publish、shared restore 或协议 payload，系统 SHALL 增加 handler smoke 或 focused flow 回归，覆盖 Local、shared 和 outbound sequence。
3. WHEN 切片完成，系统 SHALL 通过 `bash tools/run_gc_verification.sh --full` 和 `git diff --check`。

## 范围

- 阶段 1 包含：`GBE_CaptureCurrentDotaLobbyState()` 的 generic capture 边界、runtime identity 与 launch state 的 Local/shared 同步规则、对应 restore plan 和 focused flow 护栏。
- 阶段 1 包含：`state`、`game_state`、`launch_phase`、`room_name`、`connect`、`match_id`、`server_id`、`game_start_time`、options 和 `custom_game` 的现有所有权语义显式化。
- 阶段 1 排除：members、chat、broadcast、owner_hero_id 的所有权变更。
- 后续独立切片：generic metadata publish 边界、8052 lifecycle pre-write、chat postgame tombstone。
- 排除：生产 CompositionRoot、Coordinator 大拆分和真协议 L2/L4 验证恢复。
