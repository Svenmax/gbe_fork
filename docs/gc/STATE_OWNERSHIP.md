# GC 状态所有权与并发契约

## 目的

本文件以当前源码为依据记录 Dota GC 运行时状态的写入责任、读取范围、失效条件和同步域。它用于指导后续重构与上游更新，避免将多个状态对象误当作同一事实来源。

本文描述当前实现，不将目标架构视为既有行为。

## 当前状态对象

| 状态对象 | 权威写入方 | 主要读取方 | 更新入口 | 同步域 | 失效条件 |
| --- | --- | --- | --- | --- | --- |
| `GBE_local_lobby` | 当前 `Steam_Game_Coordinator` | 当前 Coordinator 的 handlers、launch、publish、restore 路径 | 协议 handler、lifecycle executor、runtime reset | Coordinator 的 `global_mutex` 域 | lobby generation 前进或 runtime clear |
| `GBE_SharedDotaLobbyState` | `dota_lobby_state::Store` | Client/Server Coordinator、restore、post-login、reconnect 上下文派生 | `publish_if_generation_current_or_newer`、`update_if_generation_current_or_newer`、`compare_update`、`compare_clear` | Store 的 `std::recursive_mutex` | `compare_clear(expected_generation)` 成功后保留 tombstone generation |
| Lobby generation | 每个 Coordinator 的 `GBE_dota_lobby_generation_counter` 与 `GBE_local_lobby.generation` | 延迟消息、deferred task、SharedLobby Store | create/join/reset/leave 等生命周期边界 | Coordinator 域；作为 Store 写入条件传递 | 计数器耗尽或新 lobby 生命周期开始 |
| `GBE_DotaReconnectContext` | reconnect context helper | serialized networking、network adapter、reconnect service | shared/local lobby 发布后派生，generic lobby recovery | 当前参与 `global_mutex` 域 | 显式 clear 或新 context 覆盖 |
| Reconnect eligibility | reconnect shared helper | networking adapter、连接流程 | lifecycle / reconnect 路径 | 当前参与 `global_mutex` 域 | consume 或显式 clear |
| Lifecycle machine state | `dota_lifecycle_state_machine` 调用方 | lifecycle executor、custom-game lifecycle handler | event decision / transition result | 调用方负责同步 | 由 Reset、Leave、Abandon、Recover 等 event 决定 |
| 延迟 GC 消息与 deferred task slot | 当前 `Steam_Game_Coordinator` | `RunCallbacks`、lifecycle handlers | queue、consume、runtime clear | Coordinator 的 `global_mutex` 域 | generation 或 lobby id 不匹配；消费完成；runtime clear |

## 源码证据

- `dll/dll/steam_game_coordinator.h:123-165` 定义 Coordinator 的 delayed message queue、generation counter、deferred task slots、去重字段和 `GBE_local_lobby`。
- `dll/gbe_dota_lobby_state_store.h:17-42` 定义共享状态 Store 的 snapshot、generation-gated 写入和 compare-update 接口。
- `dll/gbe_dota_lobby_state_store.cpp:12-72` 在 Store mutex 内实现 snapshot、publish、compare-clear 与 compare-update。
- `dll/gbe_dota_lobby_state_publish_coordinator.cpp:368-404` 从局部 lobby 构造共享快照并派生 reconnect context。
- `dll/dll/gbe_dota_reconnect_shared.h:10-41` 定义跨 Coordinator / networking 的 reconnect context 与 eligibility 接口，并说明其当前锁域。
- `dll/gbe_dota_lifecycle_state_machine.h:13-199` 定义独立的 lifecycle state、event、decision 和 effect 模型。

## 共享 Lobby Store 契约

### 已实现规则

- 所有 Store 访问通过构造时注入的共享 `Snapshot` 和 mutex 进行序列化。
- `compare_clear(expected_generation)` 仅在 generation 相等时清空共享状态，且保留 tombstone generation。
- `compare_update(expected_generation, mutator)` 在 Store 锁内复制当前 state、执行 mutator 并提交。
- `publish_if_generation_current_or_newer(state)` 拒绝低 generation 写入；当现有 state 无效而候选 state 有效且 generation 相等时也拒绝写入。
- `update_if_generation_current_or_newer(generation, mutator)` 在 Store 锁内从当前同 generation 快照派生下一状态；更高 generation 从空快照开始；相同 generation tombstone 拒绝复活。
- Store mutator 必须保持为局部数据变换，禁止调用外部 API。该规则写在 `dll/gbe_dota_lobby_state_store.h:38-41`。

### 当前风险

`GBE_PublishSharedDotaLobbyState()` 已通过 `update_if_generation_current_or_newer()` 在 Store 锁内从当前快照派生写入，避免锁外 snapshot 覆盖。`publish_local_lobby_to_shared()` 仍会按当前协议语义将 LocalLobby 的字段投影到 shared snapshot；跨角色的字段级写入集合仍需在 R4 前完成归属收敛。

### 重构前置规则

- 新的跨角色共享状态写入使用 `compare_update`、`update_if_generation_current_or_newer` 或具备等价原子语义的接口。
- 每个 Store mutator 只更新其负责的字段集合，保留同 generation 的其他字段。
- 增加并发测试，覆盖同 generation 下成员、`connect`、`server_id` 和 metadata 的交错更新。
- 每次共享状态写入明确标注来源角色、lobby id、generation 和更新字段。

## 锁域规则

### Coordinator 域

`global_mutex` 当前保护 Coordinator 局部状态、延迟消息队列与部分回调注册路径。`GBE_PublishSharedDotaLobbyState()` 在该锁内执行，见 `dll/gbe_dota_lobby_state_publish_coordinator.cpp:368-370`。

### SharedLobby Store 域

Store 使用独立的 recursive mutex。Store 方法在锁内复制、修改和写回共享快照，见 `dll/gbe_dota_lobby_state_store.cpp:12-72`。

### Reconnect 域

reconnect fallback context 和 eligibility 当前参与 `global_mutex` 同步域，见 `dll/dll/gbe_dota_reconnect_shared.h:33-41`。`GBE_DotaReconnectNetworkAdapter::queue_game_server_change()` 在 `global_mutex` 下注册延迟回调，见 `dll/gbe_dota_reconnect_network_adapter.cpp:83-100`。

### 后续约束

- 锁内工作只包含状态读取、状态转换和短生命周期数据复制。
- 网络发送、Steam callback 执行、文件 I/O 与外部 API 调用在锁外执行。
- 多锁路径必须固定获取顺序，并由测试覆盖重入与交错回调。
- 新模块不直接访问另一模块的可变状态；通过 Store、请求对象或窄接口传递数据。

## 生命周期状态模型

当前运行时并存三组生命周期信息：

- `GBE_LocalLobby` 中的 Dota `state`、`game_state`、`launch_phase` 与 launch flags，见 `dll/gbe_dota_lobby_state.h:19-87`。
- `GBE_SharedDotaLobbyState` 中供跨角色同步的状态快照，见 `dll/gbe_dota_lobby_state.h:89-142`。
- `dota_lifecycle_state_machine::MachineState` 中的抽象 lifecycle、generation 和 reconnect key，见 `dll/gbe_dota_lifecycle_state_machine.h:170-187`。

后续实现应让协议入口产生 event，state machine 输出 transition plan，executor 再更新局部、共享和延迟状态。直接跨越多组状态字段进行修改需要先补充回归用例和所有权说明。

## 变更检查清单

- [ ] 变更的字段在本文件中具有唯一写入责任人。
- [ ] 新写入使用正确的 generation 与 lobby id。
- [ ] Store 更新保持原子性并保留无关字段。
- [ ] 锁内没有网络、Steam callback、文件 I/O 或长时间计算。
- [ ] 延迟任务在执行前验证 lobby id 与 generation。
- [ ] 新的状态迁移具备 direct、wrapped 或内部 event 的对应测试。
