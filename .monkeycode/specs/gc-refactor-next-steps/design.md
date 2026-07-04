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

### 工作流 F：Post-login Routing 收口

当前 post-login dispatch 已经从大块分支推进到更清晰的路由结构，但 direct/wrapped routing、特殊 adapter、request context shaping 仍然存在分散逻辑。后续目标是把路由层稳定为轻量 registry，让新增简单 handler 的成本主要是增加 table entry。

设计边界：

- `DotaGcRequestContext` 应承载 emsg、body、wrapped、outer session、source job、target job、request path 等路由所需上下文。
- 简单 request-response handler 使用统一 adapter。
- 复杂流程 handler，例如 7034 match flow、template replay、server assignment，保留显式 adapter。
- registry 只决定“调用谁”和“如何传递上下文”，不处理业务状态机。

优先收口点：

- direct post-login if-chain 中纯 request-response 类 handler。
- wrapped/direct 同 emsg 的重复 adapter。
- unsupported emsg fallback 的日志和返回语义。

### 工作流 G：Lobby State Machine 集中化

当前 lobby state transition 已有部分 pure helper 和 state module，但 abandon、teardown、launch、reconnect、owner/member disconnect 仍散落在 chat/lobby/match/misc handler 的分支中。后续目标是逐步建立小型 transition decision 层，让 handler 读取 decision，而不是重复组合状态条件。

建议先集中以下 transition：

- Abandon/teardown：7035、7014、25、pending reset/finalize。
- Launch lifecycle：7041、7070、8052、8053、7034 runtime game_state。
- Reconnect eligibility：recent reconnect context、server id、owner connected、launch phase。
- Lobby lifecycle：create/join/leave/destroy/kick/set details 对 shared state publish 的影响。

设计边界：

- transition helper 接受 `GBE_LocalLobby`、request shape、runtime flags，返回 decision struct。
- transition helper 不发送 GC message，不修改全局状态，不触发 network/server GC。
- coordinator 或 domain handler 负责执行 decision 并记录日志。
- 先从 `compute_abandon_decision` 已有模式扩展，不直接引入完整 State Pattern。

### 工作流 H：Coordinator 和 Global State 解耦

`Steam_Game_Coordinator` 仍是 Dota GC 的胖 facade，同时跨 TU 全局状态承担了 shared lobby、pending reset、recent reconnect、launch flags 等职责。后续目标是把状态访问点收敛到小型 runtime state facade，降低 handler 对成员变量和 extern globals 的直接耦合。

候选状态分组：

- Shared lobby state：`GBE_shared_dota_lobby_state` 和 publish/snapshot 相关操作。
- Pending flow state：abandon reset、normal signout finalize、postgame teardown。
- Reconnect state：recent reconnect context、reconnect eligibility。
- Launch state：launch phase、server setup sync、host showcase equip pushed。

设计边界：

- 先建立函数级访问接口，例如 `read_pending_abandon_state`、`mark_pending_reset_after_cache_unsubscribed`。
- 在调用点稳定后，再考虑结构体封装，例如 `DotaGcRuntimeState`。
- 不在一次变更中迁移所有 globals。
- 不改变 `Steam_Game_Coordinator` 作为外部入口的角色。

### 工作流 I：Protocol Codec DTO 化

handler 中仍存在大量裸 wire 字段读取和 ad-hoc response composition。后续目标是为高风险协议建立小型 request/response DTO，让 handler 和 decision helper 处理结构化语义。

优先 DTO：

- `Dota7034RuntimeRequest`：connected/disconnected players、game_state、send_reason、kill/building state。
- `Dota7070ReadyUpRequest`：lobby id、ready state、source job。
- `Dota8052StartedLoadingRequest`：lobby id、custom game id、start time。
- `Dota8053FinishedLoadingRequest`：lobby id、duration、result code、result text、signon state。
- `Dota7035AbandonRequestContext`：wrapped、session、server/client、current lobby state。

设计边界：

- DTO parse/build 放在现有 `gbe_proto_wire` 或领域内 anonymous namespace，取决于复用范围。
- 同一 DTO 至少被 handler 和 test 共同使用时，再提取到公共 header。
- DTO 层只表达协议字段，不读取 coordinator 或 global state。

### 工作流 J：Verification Pipeline 固化

当前验证依赖人工运行脚本。后续目标是把可重复验证步骤收口成稳定 pipeline，使后续架构重构具备明确 gate。

建议 gate：

- Fast gate：`tools/run_gc_offline_tests.sh`。
- Full gate：`tools/run_gc_offline_tests.sh --full`。
- Style gate：`git diff --check`。
- Refactor audit gate：`python3 tools/_audit_gc_refactor.py`。
- Build config gate：具备 Premake 环境时运行 `premake5 gmake2`，并验证测试工程存在。

设计边界：

- 先形成本地 pre-merge checklist，再接入 CI。
- CI 只运行不需要外部服务和平台凭据的离线测试。
- 对当前环境缺失的 Premake 工具，文档中保留手动验证项。

### 工作流 K：Template/Replay 数据治理

`gbe_dota_template_replay_handlers.cpp`、payload helper 和若干 canned bytes/hex 常量仍然承载大量协议样板数据。它们对行为兼容很重要，但当前 ownership、来源和 patch 点并不总是直观。后续目标是让 template/replay 数据成为可追踪资产，降低修改 canned payload 时破坏客户端握手、lobby replay 或 post-login replay 的风险。

治理目标：

- 每个 template 常量有明确用途，例如 client welcome、server welcome、practice lobby cache subscribed、persona state、official 26 replay。
- 每个 template 的 patch 点清晰，例如 account id、steam id、lobby id、match id、owner SOID、game start time。
- template 数据的 ownership 固定在 template/replay 或 payload helper 领域，避免新增 handler 直接复制 hex blob。
- focused tests 覆盖 template patch 后的关键字段，而不是只断言 payload 非空。

设计边界：

- 不重新生成或替换 canned bytes，除非有测试证明新输出与现有客户端行为兼容。
- 不把 template 数据迁移到运行时配置文件；当前先保持编译期常量，减少部署复杂度。
- 可以新增轻量 metadata 注释或结构体索引，但不引入大型 template registry。

### 工作流 L：Logging/Trace 边界治理

Dota GC 调试高度依赖 reason string、proto boundary trace 和 response packet log。后续架构重构会移动 handler 内部逻辑，如果日志语义漂移，会降低线上问题排查能力。目标是让关键副作用 reason 稳定、可搜索、可测试。

治理目标：

- reason string 命名遵循 `emsg_or_flow_event` 风格，例如 `8053_finished_loading`、`7272_leave_chat`。
- 每个高风险 action 的 reason 表达业务触发点，而不表达临时代码结构。
- `GBE_GC_DebugLog` 负责上下文型调试输出，`GBE_LogDotaResponsePacket` 负责 outbound response 观察，proto boundary trace 负责 wire 层边界。
- handler tests 对高风险 reason 做断言，防止重构后日志语义漂移。

设计边界：

- 不把所有日志改为结构化 logging；当前代码仍以字符串日志为主。
- 不为了统一命名批量修改所有 reason；只在触及相关 handler 时同步治理。
- 不在纯 helper 中直接写日志，除非该 helper 现有职责已经是 wire/log boundary。

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
