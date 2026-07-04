# GC 重构后续实施任务清单

## 推进索引

按阶段推进，完成当前阶段的必要任务和验证后再进入下一阶段。标记为 `*` 的任务是增强项，可在必要任务完成后执行。

### Stage 0：基线保护和清理

- 覆盖任务：6、11、14、16。
- 目标：确认验证入口稳定，减少构建噪音，保证 Premake 可选测试工程和 shell 离线测试源列表一致。
- 必要验证：`tools/run_gc_offline_tests.sh`、`tools/run_gc_offline_tests.sh --full`、`git diff --check`。
- 具备工具环境追加验证：`premake5 --with-gc-tests gmake2`。
- 完成标准：默认/full 离线测试通过，Premake GC test target 仍为可选生成，任务清单记录残余平台验证风险。

### Stage 1：Inventory action-list pilot

- 覆盖任务：1。
- 目标：在 `GBE_HandleDotaEquipItemsRequest` 上验证 pure planner + explicit executor 模式。
- 必要前置：equip basic、empty、full forward 测试能证明 response、save、server GC forward、network broadcast、snapshot refresh 的顺序。
- 必要验证：默认/full 离线测试和 `git diff --check`。
- 完成标准：planner 不触达 coordinator/network/file/server GC，executor 显式保持原副作用顺序，相关 reason/job/session 断言保持通过。

### Stage 2：测试覆盖加深和 harness 收缩

- 覆盖任务：2、3、4、13.4。
- 目标：加深 7034、lobby lifecycle、launch loading 路径测试，同时控制 `stubs.h` 膨胀。
- 必要前置：新增测试只需要 recorder 字段或轻量 stub 扩展即可表达行为。
- 必要验证：默认/full 离线测试和 `git diff --check`。
- 完成标准：match/lobby/chat 关键顺序、payload 关键字段、reason 稳定性具备测试保护，harness 分区清晰。

### Stage 3：Lobby 和 launch state decision 收口

- 覆盖任务：8、9。
- 目标：把 abandon、teardown、launch、reconnect 的状态判断收口到无副作用 decision helper 和函数级 state access。
- 必要前置：对应路径已有顺序测试和 state mutation 时机断言。
- 必要验证：默认/full 离线测试、`git diff --check`，涉及声明迁移时执行 `python3 tools/_audit_gc_refactor.py`。
- 完成标准：decision helper 只返回结构化结果，handler/executor 继续显式执行 response、publish、pending flag 和外部副作用。

### Stage 4：Protocol DTO 和 routing 收口

- 覆盖任务：7、10。
- 目标：减少裸 wire 字段读取和 direct/wrapped routing 重复 adapter。
- 必要前置：DTO parse 边界和 routing fallback 具备 focused tests。
- 必要验证：默认/full 离线测试、`git diff --check`，涉及新增 helper 或声明迁移时执行审计脚本。
- 完成标准：7034、7070、8052、8053、7035 使用结构化 request/response 输入，unsupported emsg 和 job/session 透传保持测试保护。

### Stage 5：Dependency seam、persistence 和 template 治理

- 覆盖任务：12、15。
- 目标：收敛 save、network、server GC、lobby publish 等外部依赖边界，并治理 template/replay ownership。
- 必要前置：外部依赖触点和 template/replay 常量 ownership 已盘点。
- 必要验证：默认/full 离线测试、`git diff --check`，template 变更需有 focused tests 或 fixture 对齐。
- 完成标准：pure helper 不写文件、不发送网络、不触发 server GC；canned payload 的用途、patch 点和兼容风险可追踪。

### Stage 6：持续治理和 CI 固化

- 覆盖任务：5、11、14。
- 目标：在前面阶段稳定后继续收缩 internal header、固化验证 pipeline 和平台构建等价性。
- 必要前置：测试覆盖能保护被迁移的 helper 或 header 边界。
- 必要验证：默认/full 离线测试、`git diff --check`、`python3 tools/_audit_gc_refactor.py`，具备工具环境执行 Premake gate。
- 完成标准：新增文件和声明迁移进入正确构建入口，默认 CI 成本保持可控，可选 GC test path 可验证。

## 执行规则

- 每轮只处理一个 stage 中的一个必要任务或一个增强任务。
- 测试补强和生产逻辑重构分开提交。
- 任务完成时勾选对应条目，并在条目下记录实际验证命令。
- 出现顺序、job/session、payload、reason 或 publish 时机变化时，先停在测试诊断，不继续扩大范围。
- 如果某个任务需要显著扩大 stub 行为，先拆小任务补 wire/helper 测试或 recorder 字段。

- [ ] 1. Inventory action-list pilot
  - [ ] 1.1 记录 `GBE_HandleDotaEquipItemsRequest` 当前副作用顺序
    - 目标文件：`dll/gbe_dota_inventory_handlers.cpp`
    - 对齐 `tools/gbe_dota_handler_test/smoke_test.cpp` 中 equip basic、empty、full forward 测试
    - 明确 SO update、2570 response、save、server GC forward、network broadcast、lobby snapshot refresh 的顺序约束
  - [ ] 1.2 为 equip 请求定义局部 plan 数据结构
    - 放在 `dll/gbe_dota_inventory_handlers.cpp` anonymous namespace
    - 复用 `GBE_DotaActionType` 和 `GBE_DotaActionList`
    - plan 字段覆盖 modified item ids、response body、cache version、server forward 参数、broadcast/snapshot reason
  - [ ] 1.3 提取 equip pure planner
    - planner 输入为请求 body、当前 item 列表、lobby/server 状态快照和 source job
    - planner 输出 item mutation 结果和有序 action list
    - planner 不调用 coordinator 方法、不访问 `get_steam_client()`、不发送消息、不保存文件
  - [ ] 1.4 将 coordinator equip handler 改为执行 plan
    - coordinator 负责应用 item mutation、push response、save、server GC forward、network broadcast、snapshot refresh
    - 保持现有日志、reason、payload、job/session 行为
    - 保持当前 handler tests 全部通过
  - [ ]* 1.5 补 equip planner focused tests
    - 覆盖空请求、单 item equip、多 item equip、item missing、style bitmask 输入
    - 验证 plan 中 local response 位于外部副作用之前

- [ ] 2. 扩展 match 7034 真实路径测试
  - [ ] 2.1 构造 7034 connected player 请求 body
    - 目标文件：`tools/gbe_dota_handler_test/smoke_test.cpp`
    - 使用现有 varint/length-delimited helper 构造真实 wire body
    - 断言 member runtime state、hero id、response payload 和 source job
  - [ ] 2.2 构造 7034 disconnected player 请求 body
    - 覆盖 disconnected player 导致的 member runtime state 更新
    - 断言 queued lobby update 或 response 的顺序
  - [ ] 2.3 覆盖 7034 game_state runtime update
    - 构造包含 game_state/send_reason 的请求
    - 断言 26 runtime lobby details update 的 emsg、source job 和 payload 非空
  - [ ] 2.4 覆盖 7034 launch poll 最小路径
    - 设置 `GBE_local_lobby.launch_phase` 为 run queued 或 loaded 前状态
    - 断言 launch poll 不破坏 7070/8052/8053 已有顺序测试

- [ ] 3. 扩展 lobby lifecycle 顺序测试
  - [ ] 3.1 为 7042 leave lobby 添加最小 handler 测试
    - 覆盖 response、CacheUnsubscribed、shared state publish 或 reset 标志
    - 断言 reason 和 session/wrapped 保留
  - [ ] 3.2 为 8246 destroy lobby 添加最小 handler 测试
    - 覆盖 host destroy 路径的 response 和 state cleanup
    - 断言 lobby id、pending reset 或 suppress 标志
  - [ ] 3.3 为 7047 kick 添加最小 handler 测试
    - 覆盖 kick target 解析和 response/publish 顺序
    - 使用 stub 记录 target steam id 或 account id
  - [ ] 3.4 为 7050 set details 添加最小 handler 测试
    - 覆盖 lobby metadata mutation 发生在 details update publish 前
    - 断言 room name、server region 或 custom game 字段

- [ ] 4. 收缩 handler test harness 边界
  - [ ] 4.1 整理 `stubs.h` 内部 section
    - 按 core、inventory、chat/lobby、match、free globals 分区
    - 只移动声明和注释，不改变行为
  - [ ] 4.2 抽出可复用 wire body builder helper
    - 目标文件：`tools/gbe_dota_handler_test/smoke_test.cpp`
    - 统一 varint、fixed32、fixed64、length-delimited、nested message 构造
    - 保持现有测试输入字节不变
  - [ ] 4.3 评估是否拆分 stub header
    - 如果 `stubs.h` 继续超过可维护范围，拆成 core/inventory/lobby/match header
    - 每次拆分只移动声明，不新增测试语义
  - [ ]* 4.4 为 ActionRecorder 增加字段读取 helper
    - 封装 emsg mask、header job 读取、payload 非空断言
    - 降低 smoke test 重复代码

- [ ] 5. 收缩 `gbe_dota_gc_internal.h` 共享边界
  - [ ] 5.1 盘点当前 internal header 声明
    - 按 wire payload、item payload、lobby payload、stateful orchestration、logging、extern data 分类
    - 输出保留、迁移、删除清单到本任务文档或代码注释
  - [ ] 5.2 为 payload helper 建立更窄 header
    - 视调用点拆分或复用 `gbe_dota_payload_wire_helpers.h`、`gbe_dota_payload_item_helpers.h`、`gbe_dota_payload_lobby_helpers.h`
    - 更新调用方 include
  - [ ] 5.3 下沉 file-local helper 声明
    - 能进入 anonymous namespace 的 helper 不留在 internal header
    - 删除迁移后失去调用点的 stale declarations
  - [ ] 5.4 运行审计验证
    - 执行 `python3 tools/_audit_gc_refactor.py`
    - 执行 `tools/run_gc_offline_tests.sh --full`

- [ ] 6. 清理编译警告和构建配置验证
  - [ ] 6.1 修复 payload helper `extern const char *` 初始化警告
    - 目标文件：`dll/gbe_dota_gc_payload_helpers.cpp`
    - 同步 header declaration 和 definition 的 const-correctness
    - 保持 payload helper tests 通过
  - [ ] 6.2 校验测试脚本和 Premake 源列表一致
    - 对比 `tools/run_gc_offline_tests.sh` 和 `premake5.lua` 中两个 test target 的源列表
    - 确保新增测试依赖同时进入 shell 和 `--with-gc-tests` Premake 配置
  - [ ] 6.3 在具备工具环境验证 Premake 生成
    - 执行 `premake5 --with-gc-tests gmake2`
    - 验证 `tool_gbe_dota_gc_payload_helpers_test` 和 `tool_gbe_dota_handler_test` 工程存在
  - [ ] 6.4 精简 touched files include
    - 只处理本轮修改过的 domain 文件和 test harness 文件
    - 避免跨领域批量 include 清理造成审查困难

- [ ] 7. Post-login routing 收口
  - [ ] 7.1 盘点 direct/wrapped routing 重复分支
    - 目标文件：`dll/gbe_dota_post_login_handlers.cpp`
    - 输出 simple adapter、special adapter、fallback 三类清单
    - 保留 7034、template replay、server assignment 等复杂路径的显式 adapter
  - [ ] 7.2 扩展统一 request context
    - 确认 `DotaGcRequestContext` 或等价结构包含 emsg、body、wrapped、outer session、source job、target job、request path
    - 避免 handler 直接重新解析路由层上下文
  - [ ] 7.3 将简单 direct handler 迁入 routing registry
    - 优先迁移 misc minimal response、rank/profile、inventory simple request-response 路径
    - 保持日志字段和 fallback 返回语义
  - [ ]* 7.4 增加 routing registry tests
    - 验证 unsupported emsg 返回 false
    - 验证 wrapped/direct 同 emsg adapter 保持 body、job、session 透传

- [ ] 8. Lobby state machine 集中化
  - [ ] 8.1 扩展 abandon/teardown transition decision
    - 基于现有 `compute_abandon_decision` 模式
    - 覆盖 7035、7014、25、pending reset/finalize 的 decision fields
    - helper 不发送消息、不写全局状态
  - [ ] 8.2 提取 launch lifecycle transition decision
    - 覆盖 7041、7070、8052、8053、7034 runtime game_state
    - 输出 next state、next game_state、launch phase、publish reason
  - [ ] 8.3 提取 reconnect eligibility decision
    - 覆盖 recent reconnect context、server id、owner connected、launch phase
    - 保持现有 reconnect payload 行为
  - [ ]* 8.4 为 transition helpers 添加 focused tests
    - 覆盖 valid/invalid transition、stale lobby、owner disconnect、launch failed before connect

- [ ] 9. Coordinator 和 global state 解耦
  - [ ] 9.1 盘点 Dota GC extern/global state 读写点
    - 覆盖 shared lobby、pending reset、recent reconnect、launch flags、host showcase flags
    - 输出按状态分组的读写调用点清单
  - [ ] 9.2 为 pending flow state 建立函数级访问接口
    - 先封装 abandon reset、normal signout finalize、postgame teardown 标志
    - 调用方通过函数读写，不直接访问 extern global
  - [ ] 9.3 为 reconnect state 建立函数级访问接口
    - 封装 recent reconnect context 和 reconnect eligibility
    - 保持现有 payload helper 与 lobby state helper 行为
  - [ ] 9.4 评估 `DotaGcRuntimeState` 结构体迁移门槛
    - 至少两个状态组稳定通过函数访问后再考虑结构体封装
    - 不一次性迁移所有 globals

- [ ] 10. Protocol codec DTO 化
  - [ ] 10.1 定义 7034 runtime request DTO
    - 覆盖 connected/disconnected players、game_state、send_reason、kill/building state
    - parse 逻辑不读取 coordinator 或 global state
  - [ ] 10.2 定义 7070/8052/8053 launch DTO
    - 覆盖 ready state、lobby id、custom game id、start time、duration、result code、result text
    - handler 使用 DTO 字段替代裸 `read_uint*_field` 分散读取
  - [ ] 10.3 定义 7035 abandon request context DTO
    - 覆盖 wrapped、session、server/client、lobby state、game state、launch phase
    - 作为 transition decision 输入
  - [ ]* 10.4 为 DTO parse 添加 malformed wire tests
    - 覆盖空 body、truncated varint、unknown wire type、字段重复

- [ ] 11. Verification pipeline 固化
  - [ ] 11.1 新增本地 pre-merge checklist 文档
    - 记录 fast/full/style/audit/Premake gate
    - 放在 `.monkeycode/specs/gc-refactor-next-steps/` 或 `.monkeycode/docs/`
  - [ ] 11.2 将验证命令收口到脚本或 Make target
    - 保留 `tools/run_gc_offline_tests.sh` 作为主入口
    - 增加可选 audit/style wrapper 时避免破坏现有脚本行为
  - [ ] 11.3 评估 CI 接入条件
    - 只包含离线测试和无需凭据的检查
    - Premake gate 使用 `--with-gc-tests` 并在 CI 镜像具备工具时启用

- [ ] 12. Template/Replay 数据治理
  - [ ] 12.1 盘点 template/replay 常量 ownership
    - 覆盖 `gbe_dota_template_replay_handlers.cpp` 和 payload helper 中的 canned bytes/hex 常量
    - 为 client welcome、server welcome、practice lobby cache subscribed、persona state、official 26 replay 分类
  - [ ] 12.2 标注文档化 patch 点
    - 记录 account id、steam id、lobby id、match id、owner SOID、game start time 等 patch 字段
    - 优先使用代码附近注释或小型 metadata 表达，不引入大型 registry
  - [ ] 12.3 增加 template patch focused tests
    - 覆盖 patch 后关键字段可解析
    - 保持现有 replay fixture 输出不变
  - [ ] 12.4 防止新 handler 复制 hex blob
    - 新增 template/replay 数据时优先放在 template/replay 或 payload helper 领域
    - handler 通过 helper 调用，不直接持有大型 canned bytes

- [ ] 13. Logging/Trace 边界治理
  - [ ] 13.1 盘点高风险 reason string
    - 覆盖 inventory equip、chat leave、lobby abandon、match 7034、7070、8052、8053
    - 输出保留、重命名、补断言清单
  - [ ] 13.2 统一新增 reason 命名规则
    - 使用 `emsg_or_flow_event` 风格
    - reason 表达业务触发点，避免绑定临时代码结构
  - [ ] 13.3 梳理日志函数职责边界
    - `GBE_GC_DebugLog` 保留上下文调试输出
    - `GBE_LogDotaResponsePacket` 保留 outbound response 观察
    - proto boundary trace 保留 wire 层边界观察
  - [ ]* 13.4 增加 reason stability tests
    - 对高风险 handler 的关键 action reason 做断言
    - 避免重构后日志语义漂移

- [ ] 14. 平台和构建等价性治理
  - [ ] 14.1 盘点 GC 相关源列表入口
    - 覆盖 `tools/run_gc_offline_tests.sh`、`premake5.lua`、生产 project 和 workflow matrix
    - 记录新增 `.cpp` 应进入哪些入口，测试专用 `.cpp` 只进入可选 test target
  - [ ] 14.2 验证默认 Premake 路径保持轻量
    - 默认 `premake5 gmake2` 不生成 GC test targets
    - `premake5 --with-gc-tests gmake2` 生成两个 GC test targets
  - [ ] 14.3 维护 Windows/Linux 构建差异清单
    - 盘点路径、宏、include 顺序、链接顺序相关风险
    - 涉及跨平台宏时补充对应离线或 CI 验证说明
  - [ ]* 14.4 评估 PR CI smoke gate
    - 优先接入无需凭据的 GC offline fast gate
    - 保持默认 project matrix 构建时间可控

- [ ] 15. 依赖 seam、持久化和错误边界治理
  - [ ] 15.1 盘点 handler 外部依赖触点
    - 覆盖 `Steam_Client`、network broadcast、server GC forward、settings、item save、lobby publish
    - 按 pure planner 输入、executor 副作用、global runtime state 三类记录
  - [ ] 15.2 建立函数级 dependency seam
    - 先封装 item save、server GC forward、network broadcast、lobby publish 等高风险副作用
    - seam 记录调用参数并保持显式顺序
  - [ ] 15.3 明确数据 ownership 和生命周期
    - 覆盖 shared lobby、item cache、recent reconnect context、pending flags、template replay bytes
    - 记录线程访问假设和不得跨 action 持有的引用
  - [ ] 15.4 收口 persistence 触发点
    - 确认 save 只在 executor/coordinator 层触发
    - pure decision/helper 返回保存意图，不直接写文件
  - [ ] 15.5 建立错误分类和 fallback 清单
    - 覆盖 parse failure、missing lobby、missing item、server GC unavailable、template patch failure
    - 对每类错误明确 response、log、state mutation 和 return value 语义
  - [ ] 15.6 建立 replay/template 性能和 fixture 版本治理
    - 为大型 payload/template 避免重复拷贝设定局部约束
    - 为 canned payload 添加来源、用途或版本注释
    - 变更 fixture 时同步 focused tests

- [ ] 16. 检查点 - 确保所有测试通过
  - 确保所有测试通过,如有疑问请询问用户
  - 执行 `tools/run_gc_offline_tests.sh`
  - 执行 `tools/run_gc_offline_tests.sh --full`
  - 执行 `git diff --check`
  - 涉及声明迁移时执行 `python3 tools/_audit_gc_refactor.py`
  - 检查 `git status --short`，确认只包含本轮目标文件变更
