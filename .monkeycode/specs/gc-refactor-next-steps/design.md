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
- `premake5.lua` 通过 `--with-gc-tests` 可选生成 `tool_gbe_dota_gc_payload_helpers_test` 和 `tool_gbe_dota_handler_test` 两个测试工程，默认主工程生成路径保持轻量。

### 已知限制

- 当前环境缺少 `premake5` 和 `lua`，Premake 工程配置只能通过代码审阅和离线脚本间接验证；具备工具的环境需额外验证 `premake5 --with-gc-tests gmake2`。
- handler test harness 仍依赖较大的 `stubs.h`，后续继续扩展时需要控制 stub 边界。
- `gbe_dota_gc_payload_helpers.cpp` 中仍有 `extern const char *... =` 形式的既有编译警告，测试通过但输出噪音较大。
- 部分 lobby/match 路径只覆盖最小代表性顺序，尚未覆盖 7034 runtime 的复杂 payload 字段和更多 lobby lifecycle 分支。

## 设计目标

1. 将高风险 handler 转为可审查的 pure decision/helper + coordinator executor 结构。
2. 用 handler smoke tests 固化当前副作用顺序，防止逻辑重构改变协议行为。
3. 收紧测试 harness 的领域边界，降低 stub 膨胀和 SDK drift 风险。
4. 逐步收缩 `gbe_dota_gc_internal.h` 的共享声明边界。
5. 保持每一步小批量可验证，默认验证命令为 `tools/run_gc_offline_tests.sh --full` 和 `git diff --check`。

## 推进原则

后续执行以“先保护行为，再移动逻辑”为主线。每轮变更都应能回答三个问题：当前保护了哪个可观察行为、移动了哪个逻辑边界、验证命令证明了哪些不变式。

执行约束：

- 每轮只选择一个 stage、一个 handler 或一个小型共享边界。
- 进入逻辑重构前，先补齐对应 handler 的顺序测试或 payload/helper focused tests。
- 生产入口保持 `Steam_Game_Coordinator` 对外签名稳定，内部逐步收口。
- 副作用执行顺序保持显式代码路径，避免隐藏到析构、回调、全局 setter 或通用框架中。
- 大型 canned payload、template replay bytes、protobuf patch 行为先补 ownership 和 focused tests，再考虑迁移。
- 发现测试 harness 需要大幅增加 fake 行为时，优先收缩 harness seam 或补 wire/helper 测试。
- 每轮结束同步更新 `tasklist.md` 的状态、验证命令和残余风险。

## 阶段路线

### Stage 0：基线保护和清理

目标是让后续每一步具备稳定验证入口。优先完成构建警告清理、Premake 可选测试工程验证、测试脚本和工程源列表一致性检查。

进入条件：当前分支离线测试通过，工作区只包含本轮目标文件。

退出条件：默认/full GC 离线测试通过，`git diff --check` 通过，涉及 Premake 的变更在具备工具环境记录验证结果。

### Stage 1：Inventory pilot

目标是在 `GBE_HandleDotaEquipItemsRequest` 上验证 plan/executor 模式。该 handler 已有较强测试覆盖，适合作为后续逻辑收口模板。

进入条件：equip basic、empty、full forward 测试能证明当前副作用顺序、server GC 参数、job/session 和 reason。

退出条件：planner 无 I/O 副作用，executor 保持原顺序执行，相关 handler tests 和 full offline tests 通过。

### Stage 2：测试覆盖加深和 harness 收缩

目标是继续加深 match/lobby/chat 关键路径测试，同时控制 `stubs.h` 膨胀。该阶段为 lobby state machine 和 protocol DTO 化提供安全垫。

进入条件：新增测试能使用现有 recorder 或轻量字段扩展表达关键行为。

退出条件：7034、lobby lifecycle、7070/8052/8053 的关键顺序和 payload 字段具备回归保护，harness section 或拆分边界清晰。

### Stage 3：Lobby 和 launch state decision 收口

目标是把 abandon、teardown、launch、reconnect 等状态判断提取成小型 decision helper。handler 继续负责执行和日志。

进入条件：对应路径已有顺序测试和 state mutation 时机断言。

退出条件：decision helper 只返回结构化决策，handler 按原顺序执行 response、publish、pending flag、network/server GC 副作用。

### Stage 4：Protocol DTO 和 routing 收口

目标是减少裸 wire 字段读取和 direct/wrapped routing 重复逻辑。优先处理 7034、7070、8052、8053、7035 的 request DTO 和简单 request-response adapter。

进入条件：DTO parse 失败、字段缺失、重复字段、truncated varint 等边界有 focused tests。

退出条件：handler 使用结构化 DTO 或 request context，unsupported emsg fallback、wrapped/direct job/session 透传保持测试保护。

### Stage 5：Global state、dependency seam 和 persistence 边界

目标是收敛 extern/global state 读写点，并把 save、network、server GC、lobby publish 等外部依赖放入显式 seam。

进入条件：已有状态读写点清单，目标状态组对应 handler tests 覆盖关键行为。

退出条件：至少一个状态组通过函数级访问接口读写，persistence 只在 executor/coordinator 层触发，错误分类和 fallback 语义有文档或测试覆盖。

### Stage 6：持续治理和 CI 固化

目标是把已经稳定的离线测试、审计和 Premake gate 收口为可重复执行流程，并持续治理 template/replay、logging/trace、平台构建等价性。

进入条件：本地脚本和文档 checklist 已稳定使用。

退出条件：适合 CI 的 gate 已明确，默认主 CI 成本可控，`--with-gc-tests` 路径在工具可用环境可验证。

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
- 在具备工具的环境中实际运行 `premake5 --with-gc-tests gmake2`，验证可选测试工程。

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
- Build config gate：具备 Premake 环境时运行 `premake5 --with-gc-tests gmake2`，并验证测试工程存在；默认 Premake 生成路径保持主工程轻量。

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

### 工作流 M：平台和构建等价性

当前离线测试主要在 Linux shell 路径验证，生产构建和 PR CI 以 Windows Premake/MSBuild 路径为主。后续架构重构涉及新增文件、header 拆分和可选测试工程时，需要把平台差异作为显式风险管理对象。

治理目标：

- 新增源文件同时进入 shell test、Premake test target 和生产 project 的正确源列表。
- Windows/Linux 路径分隔、宏条件、include 顺序和链接顺序保持可审查。
- 可选 GC test target 只在 `--with-gc-tests` 下生成，主 CI 的默认 project matrix 保持当前成本模型。
- 每次触及 Premake、workflow 或跨平台宏时，记录本地可执行验证和需要 CI 镜像验证的部分。

设计边界：

- 平台兼容修复以最小条件分支为主，避免为了测试支架引入生产编译宏扩散。
- 测试工程需要复用真实生产 `.cpp`，避免形成独立的替代实现。
- CI 接入先从离线 smoke gate 开始，Premake GC test gate 依赖工具可用性单独启用。

### 工作流 N：依赖注入 seam 和持久化边界

当前 handler 仍直接触达 `Steam_Client`、networking、settings、file save、global runtime state 和 template replay 数据。后续 plan/decision 重构需要先建立小型 seam，让 pure planner 能读取快照并返回副作用意图，executor 再统一触达外部依赖。

治理目标：

- 为 `Steam_Client`、network broadcast、server GC forward、item save、lobby publish 建立最小函数级 seam。
- 明确数据 ownership、生命周期和线程访问假设，尤其是 shared lobby、item cache、recent reconnect context、pending flags。
- 将 save/persistence 触发点从 decision helper 中剥离，集中在 executor 或 coordinator 层。
- 建立错误分类和 fallback 语义，例如 parse failure、missing lobby、missing item、server GC unavailable、template patch failure。
- 为 replay/template payload 和大型 cached bytes 建立性能预算与内存复制约束，避免重构时引入重复拷贝。
- 为测试 fixture 和 canned payload 建立版本标记或来源注释，便于协议数据更新时判断兼容性。

设计边界：

- 先提取函数级 seam，再评估对象级 dependency injection。
- seam 以生产行为稳定为先，测试 harness 通过记录参数验证副作用，而不是替换核心决策。
- persistence 和 network seam 保持显式调用顺序，避免隐藏在析构、回调或全局 setter 中。

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
premake5 --with-gc-tests gmake2
```

## 风险控制

- 每次只改一个 handler 或一个小领域。
- 每次重构前先写或确认顺序测试。
- 纯 helper 先放在当前 domain `.cpp` anonymous namespace，等复用明确后再提取文件。
- 不通过“让 stub 返回成功”掩盖真实行为；stub 应记录参数，测试断言关键字段。
- 遇到测试难以表达的真实 protobuf 行为时，优先补 wire/helper 测试，再推进 handler 重构。

### 停止推进信号

出现以下任一情况时，本轮应停在测试或诊断层，不继续扩大重构范围：

- 新增测试需要大量模拟生产逻辑才能通过。
- handler 重构导致 action 顺序、job/session、reason、payload size 或 publish 时机出现非预期变化。
- planner 开始直接调用 coordinator、network、file save、server GC 或 global setter。
- 为了复用而引入跨领域公共 executor，但只有一个调用点。
- canned payload 输出变化缺少 focused test 或 fixture 兼容说明。

### 回退策略

每轮变更应保持可局部回退：测试补强提交和生产逻辑重构提交分开；当生产逻辑重构失败时，保留新增测试和文档，回退对应生产改动后重新选择更小切片。

## 交付标准

一个后续批次可以完成的标准：

- 代码变更集中在一个领域或一个清理主题。
- 新增或更新测试能证明关键副作用顺序。
- 默认和完整离线测试通过。
- `git diff --check` 通过。
- 若迁移声明或新增文件，审计脚本通过。
- 文档任务清单同步更新，已完成项勾选或移出后续清单。
