# GC 重构后续技术方案

## 背景

当前 Dota GC 重构已经完成主要机械拆分、payload helper 拆分、post-login dispatch table、lobby state transition 收口、handler 级离线测试支架扩展。最新测试覆盖已经能编译并执行 inventory、chat、lobby、match、misc handler 的代表性路径。

后续工作的核心目标是把已有的文件拆分成果继续推进为可维护的逻辑边界：每个高风险 handler 的请求解析、状态决策、消息构造和副作用执行应逐步分离。副作用顺序必须保持稳定，尤其是 Dota 客户端敏感的 GC message 顺序、job/session 传递和 lobby state publish 时机。

## 当前基线

### 已完成能力

- `tools/run_gc_offline_tests.sh` 默认覆盖高价值 GC 离线测试。
- `tools/run_gc_offline_tests.sh --full` 覆盖完整 GC 离线测试套件。
- `tools/gbe_dota_gc_payload_helpers_test` 覆盖 payload helper、wire helper、item helper、lobby payload helper 的真实路径和边界路径。
- `tools/gbe_dota_handler_test` 已接入真实 handler TU：inventory、misc、chat、lobby、match。
- handler test harness 已记录关键副作用字段：action type、emsg、payload、wrapped、session、source job、target job、reason、server GC forward 参数。
- `premake5.lua` 已包含 `tool_gbe_dota_gc_payload_helpers_test` 和 `tool_gbe_dota_handler_test` 两个测试工程。

### 已知限制

- 当前环境缺少 `premake5` 和 `lua`，Premake 工程配置只能通过代码审阅和离线脚本间接验证。
- handler test harness 仍依赖较大的 `stubs.h`，后续继续扩展时需要控制 stub 边界。
- `gbe_dota_gc_payload_helpers.cpp` 中仍有 `extern const char *... =` 形式的既有编译警告，测试通过但输出噪音较大。
- 部分 lobby/match 路径只覆盖最小代表性顺序，尚未覆盖 7034 runtime 的复杂 payload 字段和更多 lobby lifecycle 分支。

## 设计目标

1. 将高风险 handler 转为可审查的 pure decision/helper + coordinator executor 结构。
2. 用 handler smoke tests 固化当前副作用顺序，防止逻辑重构改变协议行为。
3. 收紧测试 harness 的领域边界，降低 stub 膨胀和 SDK drift 风险。
4. 逐步收缩 `gbe_dota_gc_internal.h` 的共享声明边界。
5. 保持每一步小批量可验证，默认验证命令为 `tools/run_gc_offline_tests.sh --full` 和 `git diff --check`。

## 非目标

- 不一次性引入大型 class hierarchy、Command framework 或通用 executor 框架。
- 不在缺少测试保护的情况下重写 wire payload 格式。
- 不把所有 handler 统一成同一种抽象；只在重复模式清晰后提取公共结构。
- 不为了减少文件数量而重新合并已经按领域拆分的 handler 文件。

## 推荐架构方向

### Handler 内部结构

每个高风险 handler 保持 `Steam_Game_Coordinator` 对外入口不变，内部逐步拆成三段：

1. Parse/Read 阶段

读取 protobuf wire 请求、outer session、source job、当前 lobby/item 状态，生成小型输入结构。

2. Plan/Decision 阶段

使用纯函数计算状态变化、响应 payload、待执行副作用顺序。纯函数不得调用 `push_incoming_now`、`save_items_to_file`、server GC、network broadcast、lobby snapshot replay。

3. Execute 阶段

coordinator 按 plan 中定义的顺序执行真实副作用，并保持现有日志、job/session 和 wrapped 语义。

### Action List 使用边界

`dll/gbe_dota_action_model.h` 已定义 `GBE_DotaActionType` 和 `GBE_DotaActionList`。后续使用规则：

- 只有一个 handler 有多个可观察副作用且顺序敏感时，才引入 action list。
- 单一 response 或简单状态写入路径继续保持直接实现。
- action list 应描述副作用，不持有 coordinator 指针，不直接执行 I/O。
- executor 可以先局部存在于对应 domain `.cpp` 的 anonymous namespace，等至少两个 handler 复用后再提取公共 executor。

### 测试策略

handler 级测试优先验证协议敏感事实：

- action 顺序。
- emsg 和 protobuf mask。
- source job、target job 和 outer session 保留。
- payload 非空和关键字段可解析。
- lobby/item 状态变化发生在 publish/response 前后正确位置。
- server GC forward 参数和 reason。

payload/helper 级测试继续验证纯函数：

- malformed varint、truncated field、unknown wire type。
- owner/account/lobby/match id patch。
- item serialization、equip state、style bitmask。
- lobby cache/details/launch replay payload。

## 后续工作分组

### 工作流 A：Inventory Action-List Pilot

Inventory 是最适合作为下一轮逻辑收口模板的领域，因为现有测试已覆盖 unlock、set style、equip、server GC forward 和 network broadcast。目标是把 `GBE_HandleDotaEquipItemsRequest` 的多副作用路径先转成局部 plan/executor 模式，再评估是否推广到 unlock/set style。

关键不变式：

- equip local response 必须在 server GC forward、network broadcast、snapshot refresh 前。
- item mutation、save、callback 顺序必须保持现有测试记录。
- full item cache forward 仍需在 server GC create/update 之前。

### 工作流 B：Chat/Lobby/Match 覆盖加深

当前 chat/lobby/match 已有 smoke tests，后续应把代表性路径扩展到更真实 payload 和更多分支。

优先顺序：

- 7034 direct match request：connected/disconnected players、game_state、send_reason、runtime update。
- 7035 abandon：current-game disconnect、postgame teardown、pending reset/finalize 标志。
- lobby lifecycle：join/leave/destroy/kick 的 24/25/26 和 publish 顺序。
- 7070/8052/8053：ready/loading/finished loading 的 launch phase 和 payload 字段。

### 工作流 C：Test Harness 边界收缩

`tools/gbe_dota_handler_test/stubs.h` 当前承载所有领域 stub。后续继续增加覆盖前，先按领域拆分或至少按 section 收口，避免单文件继续膨胀。

建议结构：

- `handler_core_stubs.h`：基础 SDK 类型、ActionRecorder、Steam_Client、Settings、Networking。
- `handler_inventory_stubs.h`：inventory 所需声明和 helpers。
- `handler_lobby_stubs.h`：lobby/chat 所需 declarations 和 lobby state globals。
- `handler_match_stubs.h`：match/launch 所需 declarations 和 launch coordinator helpers。

可以先不拆文件，只在 `stubs.h` 中保持同样的逻辑分区。真正拆文件应在一次小 PR 中完成，并只移动声明，不改变测试行为。

### 工作流 D：Internal Header 收缩

`gbe_dota_gc_internal.h` 仍承担较多跨 TU 声明。后续应按 payload、wire、lobby state、logging、stateful orchestration 分组收缩。

目标是让每个 domain `.cpp` 只 include 自己需要的最小 header，减少新增 helper 自动进入大 internal header 的惯性。

### 工作流 E：质量清理

继续处理不影响行为但影响维护的质量问题：

- 修复 `extern const char *... =` 编译警告。
- 清理重复 include。
- 将 handler test 工程配置和 shell script 源列表保持一致。
- 在具备工具的环境中实际运行 Premake 生成，验证新增测试工程。

## 验证矩阵

每个后续批次至少执行：

```bash
tools/run_gc_offline_tests.sh
```

```bash
tools/run_gc_offline_tests.sh --full
```

```bash
git diff --check
```

涉及声明迁移或新增/删除 helper 时执行：

```bash
python3 tools/_audit_gc_refactor.py
```

涉及 Premake 配置时，在具备工具的环境执行：

```bash
premake5 gmake2
```

## 风险控制

- 每次只改一个 handler 或一个小领域。
- 每次重构前先写或确认顺序测试。
- 纯 helper 先放在当前 domain `.cpp` anonymous namespace，等复用明确后再提取文件。
- 不通过“让 stub 返回成功”掩盖真实行为；stub 应记录参数，测试断言关键字段。
- 遇到测试难以表达的真实 protobuf 行为时，优先补 wire/helper 测试，再推进 handler 重构。

## 交付标准

一个后续批次可以完成的标准：

- 代码变更集中在一个领域或一个清理主题。
- 新增或更新测试能证明关键副作用顺序。
- 默认和完整离线测试通过。
- `git diff --check` 通过。
- 若迁移声明或新增文件，审计脚本通过。
- 文档任务清单同步更新，已完成项勾选或移出后续清单。
