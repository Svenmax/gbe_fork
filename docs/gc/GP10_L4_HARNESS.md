# GP-10 L4 Harness 设计（E4）

> 对照代码日期：2026-07-13。**设计已完成；S0 spike 失败 → L4 实现书面延期（见 §13）。**
> 主入口：`GOLDEN_PATHS.md` GP-10、`PHASE_E_BOUNDARY.md` E4/E5、`PHASE_E_EXIT.md`。
> 相关：`MESSAGE_ROUTING_INVENTORY.md`、`HOST_AUTHORITY.md`、`dual_gc_host_test`、`composition_root`。

## 1. 目标与非目标

| | 内容 |
|--|------|
| **目标** | 进程内两个 GC 角色（client + gameserver）经 **真实入队/出队 API** 完成至少一条可重复往返，断言消息序与关键 shared/local 状态，而不是只测 pure 规则或 stub recorder。 |
| **完成信号** | 独立 offline 二进制进 `run_gc_verification --full`；至少 1 条场景绿；GOLDEN GP-10 从「缺」改为「部分」或「有」；本文件 §7 勾选。 |
| **非目标** | 全量 Dota 协议回归；真实 Steam 网络 / 跨进程；CompositionRoot 进生产；为 L4 拆 `Steam_Game_Coordinator`；宣称 L4 但仅 mock 无双队列。 |

## 2. 级别对照（为何 L3 不够）

| 级别 | 现有资产 | 覆盖 | 缺口 |
|------|----------|------|------|
| L2 | `gbe_dota_handler_test` stub `Steam_Game_Coordinator` + recorder | 单侧 handler 副作用序 | 无真 `SendMessage_`/`RetrieveMessage` 环；server 多为第二 stub |
| L3 | `gbe_dota_dual_gc_host_test` | **纯** Store publish/adopt、showcase/wearable one-shot（H1–H5） | **不链** 完整 `Steam_Game_Coordinator` |
| CompositionRoot | `gbe_dota_composition_root_test` | Store + registry 副本 + reconnect 角色依赖 | **不构造** 生产 GC 实例 |
| **L4（本设计）** | **尚缺** | client/server 各一队列；入口 `SendMessage_` 或等价生产路径；出口 `IsMessageAvailable`/`RetrieveMessage` 或对称 drain | 见 §4 成本 |

**L4 最低语义：** 至少一侧调用生产分发入口，另一侧（或同侧）从 **真实 message queue** 读出期望 emsg，且双方共享与生产一致的 **一个** `dota_lobby_state::Store`（或明确记录为何不能共享）。

## 3. 生产装配事实（不可违背）

1. **生产构造**在 `dll/steam_client.cpp`：`steam_game_coordinator`（client）与 `steam_gameserver_game_coordinator`（server）各 `new Steam_Game_Coordinator(...)`，共享 `GBE_GetSharedDotaLobbyStateStore()` 与同一 registry 视图。
2. **公共 API**：`SendMessage_` → `handle_dota_client_message`；出站入队 `push_incoming` / `push_incoming_now`；消费 `IsMessageAvailable` / `RetrieveMessage`。
3. **Hello 轨**不进 post-login registry（见 `MESSAGE_ROUTING_INVENTORY`）；L4 若测 Hello 走 4006/4007 直达 welcome handlers。
4. **CompositionRoot** 仅 offline 所有权模型；**禁止**为 L4 把 CompositionRoot 挂进生产路径。
5. **offline 闸门**以 `tools/run_gc_offline_tests.sh` 源列表为准；全量链生产 GC 会显著拉高链接面与依赖（Settings/Network/Callbacks/LocalStorage/protobuf）。

## 4. 推荐架构（分档）

### 4.1 档 A — 薄双队列环（首选首实现）

**形态：** 不启完整 `Steam_Client`；在测试进程内构造 **两个** `Steam_Game_Coordinator`（`is_server=false/true`），注入：

- 共享 `dota_lobby_state::Store`（与 dual_gc / 生产语义一致）
- 最小 `Settings`（steam_id / app_id=570）
- 可空或 fake 的 Network / LocalStorage / Callbacks / RunEveryRunCB / LifecycleExecutor（与 handler_test 或 composition 测同类）

**泵：**

```text
client.SendMessage_(emsg, body)
drain(client)  -> 可选交叉注入 server.push_incoming_message / server.SendMessage_
drain(server)
assert ordered emsg list + Store snapshot fields
```

**优点：** 直接打到生产类与真队列；比整颗 Client 小。
**风险：** 构造参数与成员初始化路径重；大量静态/全局依赖；可能需扩大 test source list 到接近「半生产」。
**准入：** 先做 **链接探测 spike**（单独 target 能 `new` 两个 GC 并 `SendMessage_` 一次 Hello 或 no-op）再写场景。

### 4.2 档 B — Steam_Client 进程内双 GC（备选）

**形态：** 构造/借用 `Steam_Client` 子集，使用已有 `steam_game_coordinator` + `steam_gameserver_game_coordinator`。
**优点：** 装配与生产一致。
**风险：** 依赖面最大；初始化/appid/网络回调噪声多；CI 时间与脆弱性高。
**仅当档 A spike 证明无法在合理源列表内链接时升级。**

### 4.3 档 C — 伪 L4（明确 **不算** GP-10 完成）

- 双 stub recorder 互推
- 仅 dual_gc pure + handler smoke 拼贴
- 单 GC 自环 `push_incoming` 再 `RetrieveMessage`（无第二角色）

可用于开发调试，**不得**把 GP-10 标为「有」。

## 5. 消息泵契约

| 项 | 约定 |
|----|------|
| 入站 | 优先 `SendMessage_(unMsgType, pubData, cubData)`；禁止测试里直接调私有 handler 却宣称 L4 |
| 出站 drain | 循环 `IsMessageAvailable` + `RetrieveMessage` 直到空；记录 `(masked_emsg, size)` 序 |
| 交叉投递 | server 侧若需消费 client 已入队消息：从 client drain 后 **原样** `server.SendMessage_` 或文档化的 `push_incoming_message`（后者仅当与生产跨 GC 路径一致且在设计中点名） |
| 延迟消息 | 首场景 **禁止** 依赖 `push_incoming` delay 定时器；只用 `push_incoming_now` 路径或显式 `run_callbacks` 若已存在且可测 |
| 并发 | 单线程泵；不引入 TSAN 新场景于首 PR |
| 全局锁 | 若调用路径取 `global_mutex`，测试保持与生产同序，避免双锁死锁 |

## 6. 最小场景切片（实现时按序）

| ID | 场景 | 断言 | 依赖 |
|----|------|------|------|
| S0 | **链接 spike** | 两 GC 构造/析构；client `SendMessage_` 返回 OK 或可解释错误码 | 源列表 + 最小 fake deps |
| S1 | **ServerHello 自环**（server GC） | parse 成功后队列出现 `ServerWelcome`(4005)；已 `welcome_received` 再发则 **无** 第二 welcome | E2 plan 语义；payload builder |
| S2 | **ClientHello 自环**（client GC） | 队列出现 ClientWelcome(4004)；optional top_custom / login_sync **仅当** 可 stub 数据源 | E1 plan；可能需 settings 表 |
| S3 | **Shared lobby 双角色** | client create/join 路径后 Store generation 与 server adopt 一致（可缩小为 publish/adopt + 一侧 SendMessage 触发 restore） | Store 门控；勿一次上完整 7038 全链除非 smoke 已稳 |
| S4 | **HOST 一枪**（可选） | 一侧 2569/7034 后 showcase one-shot 与 dual_gc H 规则一致 | HOST_AUTHORITY；成本高，可二期 |

**首 PR 完成定义建议：** S0 + **S1 或 S2 之一** 进闸门；S3+ 另 PR。

## 7. 完成定义清单（实现 PR 勾选）

- [ ] 新二进制（建议名 `gbe_dota_dual_gc_roundtrip_test`）列入 `run_gc_offline_tests.sh` 与 audit source-list 规则
- [ ] 两实例共享 Store；角色 `is_server` 正确
- [ ] 至少一条场景经 `SendMessage_` + drain 断言 emsg 序
- [ ] 不修改 Hello/Welcome **生产字节**仅为过测
- [ ] 不把 CompositionRoot 接入生产
- [ ] `GOLDEN_PATHS` GP-10 状态更新；`PHASE_E` §2 同步
- [ ] `bash tools/run_gc_verification.sh --full` 绿

## 8. 延期理由（可写进 CURRENT，替代「有 L4」）

满足任一条即可 **书面延期**（仍算 Phase E 出口中的「明确延期理由」）：

1. **链接 spike 失败**：在可维护源列表内无法构造双 `Steam_Game_Coordinator`（缺符号 > 约定阈值，如需整库 steam_client + network stack）。
2. **初始化成本**：最小 Settings/Network 假件无法让 `gc_profile==DOTA2` 稳定进 Hello 路径。
3. **优先级**：生产缺陷修复与 D0 纪律优先；L4 无回归痛点时保持设计态。

延期时：GP-10 保持「缺」；在 `ACTIVE_QUEUE` 写 `E4-deferred` 一行 + 本文件日期；**禁止**用档 C 冒充完成。

## 9. 硬停手

- 无 S0 spike 直接铺 S3/S4 全协议
- 为过测改 registry 映射或 welcome payload 字节
- CompositionRoot 进生产 / 真 DI 借 L4 名义落地
- 与 Phase D 上帝类大拆同一 PR
- 将 dual_gc pure 测试改名称为 L4

## 10. 与现有测的关系

| 资产 | L4 中的角色 |
|------|-------------|
| `dual_gc_host_test` | 继续守 H1–H5 pure；L4 **不替代** |
| `handler_test` smoke | 继续 L2；L4 场景应能在失败时对照 smoke PathID |
| `plan_client_hello` / `plan_server_hello` | L1.5 决策；L4 验证 **执行后** 队列与 plan 一致 |
| `CompositionRoot` | 可借鉴依赖顺序文档；**不**作为 L4 GC 宿主 |

## 11. 建议实施顺序（批准后）

1. Spike PR：源列表 + 双 GC 构造/析构 + 空 `SendMessage_` 或 S1 骨架（可 `@ignored` 若红，但不得合入红闸门）。
2. S1 或 S2 绿 → 更新 GOLDEN GP-10「部分」。
3. S3 可选；S4 仅 HOST 回归痛时。
4. 全程遵守 `CURRENT` 硬规则与 `verification --full`。

## 12. 维护

- 实现或延期后更新本节日期与 §7/§8/§13。
- 冲突时：`CURRENT` 硬规则 > 本设计 > 历史 delivery 长文。

## 13. S0 spike 结果与延期（2026-07-13）

| 项 | 内容 |
|----|------|
| 尝试 | handler_test stub 包装编译 `gbe_dota_welcome_coordinator.cpp` |
| 结果 | **失败**（缺 Hello/login-sync/prime 成员声明；callback welcome 重定义；VPK/login 依赖面） |
| 命中 §8 | 第 1 条：可维护源列表内无法构造生产双 GC 真队列 |
| 处置 | **E5 延期**；GP-10 保持「缺」；不写伪 L4；复盘见 `PHASE_E_EXIT.md` |
| 重启条件 | 可维护链接策略（窄 seam / 专用 fake 生产装配）且 S0 绿后再写 S1+ |
