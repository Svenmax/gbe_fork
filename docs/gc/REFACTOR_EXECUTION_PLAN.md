# GC 重构执行计划

## 目标

本计划以降低后续协议升级、上游同步和行为修复的维护成本为目标。阶段完成条件以行为稳定性、状态边界和可验证性衡量。

## 工作规则

- 单个提交只归属一种类型：行为修复、协议兼容、结构迁移或测试。
- 每个重构提交必须可编译并通过受影响的离线测试。
- 高风险生命周期入口的变更同时更新协议矩阵、状态所有权和 fixture。
- 上游同步提交单独保留，不混入本地行为修改。
- 新模块优先依赖窄接口或数据对象，避免向 `gbe_dota_gc_internal.h` 添加新跨模块符号。

## R1：共享 Lobby Store 原子更新

### 目标

消除同一 generation 内“读取旧快照、锁外计算、整体写回”导致的无关字段覆盖。

### 涉及模块

- `dll/gbe_dota_lobby_state_store.h`
- `dll/gbe_dota_lobby_state_store.cpp`
- `dll/gbe_dota_lobby_state_publish_coordinator.cpp`
- `tools/gbe_dota_lobby_state_store_test/`
- `tools/gbe_dota_concurrency_stress_test/`

### 实施步骤

1. [x] 将生产发布路径改为 generation-checked Store mutator，在 Store 锁内从当前 snapshot 派生下一状态。
2. [x] 保持 `compare_clear` 的 tombstone generation 语义。
3. [x] 为成员更新、`connect`、`server_id`、metadata 更新编写同 generation 交错测试。
4. [ ] 为跨角色发布定义字段级更新意图，明确局部 lobby 可写入 shared snapshot 的字段集合。
5. [ ] 保留 stale generation 诊断日志，补充更新来源和字段集合。

### 验收

- [x] 同 generation 的 Store mutator 不会擦除其他 mutator 已提交的字段。
- [x] 旧 generation 的 update 与 clear 都被拒绝；同 generation tombstone 不能被 update 复活。
- [x] 现有 Store、dual GC host、concurrency stress 测试通过。
- [ ] handler replay 与完整离线测试通过。

## R2：生产 Dota GC 装配边界

### 目标

将依赖创建、回调注册和服务绑定从 `Steam_Game_Coordinator` 的构造过程抽出，形成可测试的生产装配边界。

### 涉及模块

- `dll/steam_game_coordinator.cpp`
- `dll/dll/steam_game_coordinator.h`
- `dll/gbe_dota_composition_root.*`
- `dll/steam_client*.cpp`

### 实施步骤

1. [x] 列出 Coordinator 构造函数当前副作用：依赖保存、callback 注册、配置解析、GC 初始化。
2. [x] 将 Coordinator 构造函数缩减为接收已装配依赖。
3. [x] 在 `Steam_Client` 中集中 client/server 的依赖对象组装，保持原有创建与销毁顺序。
4. [x] 将 callback 注册、配置解析和 GC 初始化移入显式 `start()`；将 callback 解除注册移入幂等 `stop()`，启动异常时自动回滚注册。
5. [ ] 创建生产可用的 runtime assembly 对象，负责 Store、handler registry、lifecycle executor 和 reconnect service 的创建与销毁顺序。
6. [ ] 把 callback 注册、解除注册和 shutdown 顺序纳入生产集成测试。

### 验收

- [x] Coordinator 的生产构造点以具名 `Dependencies` 对象传入 Store、registry 和 executor，消除了参数位置耦合。
- [ ] 生产环境与离线测试共享相同的依赖构造规则。
- [x] Coordinator 构造函数没有隐式 GC 初始化或 callback 注册。
- [ ] 销毁顺序经过测试，延迟回调不能访问失效依赖。
- [ ] Linux 与 Windows 生产目标均完成编译。

## R3：收缩跨模块内部总线

### 目标

停止 `gbe_dota_gc_internal.h` 作为通用跨 TU 依赖出口的增长。

### 实施步骤

1. [x] 为其中现有声明标注使用方和所属能力，记录在 `internal-header-shrink-plan.md`。
2. [x] 将日志和协议边界诊断迁至 `gbe_dota_gc_diagnostics.h`；相关 payload、lobby coordinator 已直接依赖该接口。
3. [x] 将 ServerHello cache、VPK loot cache、二进制序列化和旧协议资产迁至 `gbe_dota_server_hello_cache.h`、`gbe_dota_vpk_loot_cache.h`、`gbe_dota_binary_helpers.h`、`gbe_dota_protocol_assets.h`；欢迎、模板回放、库存、payload 和 lobby helper 模块已直接依赖这些接口。
4. [x] 将 persona 外设消息构造和响应包诊断分别归入 wire helper 与 diagnostics 接口；`gbe_dota_gc_internal.h` 的业务声明与全部源码/测试 include 已移除，空兼容头已删除。
4. [ ] 新 handler 通过 capability 接口、请求对象或专用头文件获取依赖。
5. [ ] 为该内部头设置 include-count 与声明数的下降目标。

### 验收

- [x] 本轮迁移的模块不新增对 `gbe_dota_gc_internal.h` 的依赖；诊断拆分后为 27 个实现文件，ServerHello、VPK 和二进制辅助接口拆分后为 23 个实现文件，协议资产拆分后为 21 个实现文件，清空业务声明后为 17 个实现文件，最终 include 数为 0。
- [x] 已删除该内部聚合头；审计强制其保持删除且生产源码不引用它。
- [x] 受影响的纯 helper 与 handler 测试通过；离线测试在 `gbe_dota_behavior_replay` 构建前达到 120 秒运行上限。

## R4：生命周期状态单向化

### 目标

使协议事件经由 lifecycle decision 和 action executor 更新运行时状态，减少跨 LocalLobby、SharedLobby 与 deferred slots 的分散直接修改。

### 实施步骤

1. [x] 审计 create 与 join：两条路径先推进 generation，再替换 `GBE_local_lobby`；临时 `MachineState` 没有持久 lifecycle state，不能以 `Idle` gate 拒绝已有 lobby 上的重建或切换加入。
2. [x] R4.1 为 create/join 增加无状态 generation-boundary decision：只接受对应 event，验证 generation 与上限，产出 `GenerationAdvanced` effect，实际 mutation 保留在现有 `GBE_AdvanceDotaLobbyGeneration()`。
3. [x] 审计 launch-init：7041 直接替换 launch snapshot，再依次 reset 外围状态、publish shared state、发送 26；custom-game 的后续 8052/8053/7070 已有 state-machine 与 executor gate。
4. [ ] 将每条路径表示为 event、transition plan 和 executor action。
5. [ ] 让 executor 成为局部状态、共享状态和延迟任务的统一写入入口。
6. [x] 为 create/join generation boundary 增加已有 lifecycle、stale generation、generation 耗尽和错误 event 回归；保留既有 direct/wrapped 与 replay 覆盖。
7. [x] R4.2 将 7041 的 `LaunchPeripheralReset` 与 `SharedLobbyPublish` 收敛为纯 `launch_init_action_list()` 经 lifecycle executor 执行；完整 launch snapshot mutation 与 26 response 顺序保持原位。
8. [x] R4.3 将 8246 destroy 的 immediate clear 先经 `transition_runtime_clear_boundary()` 决策，再由 `destroy_lobby_reset_action_list()` 和 lifecycle executor 执行 reset；25、8247 response 顺序保持原位。
9. [x] R4.4 将 7004 signout 的 postgame state 提升、shared publish 和 details update 收敛为 `signout_postgame_action_list()` 经 lifecycle executor 执行；7005 response 与后续 postgame queue 保持原位。
10. [x] R4.5 将 7035 current-game disconnect 在取回 25 后的 deferred reset 收敛为 `abandon_disconnect_reset_action_list()` 经 lifecycle executor 执行；保留 Reset generation boundary、清空消息队列与协议取回顺序。
11. [x] R4.6 将 7004 postgame queue 后的 details update、25 response 与 pending-finalize slot 收敛为 `normal_signout_postgame_followup_action_list()` 经 lifecycle executor 执行；保持 details、25、pending 的原有顺序与 DotaResponse route。
12. [x] R2 审计收尾：Dependencies 聚合对象列出 coordinator 的全部服务依赖，构造函数显式接收该对象；架构审计同步验证这一契约。
13. [x] R3 审计收尾：将 diagnostics、VPK loot cache 与 server-hello cache 的专用能力头纳入公开符号审计，消除已声明共享 helper 的 under-exposed 误报。
14. [x] D0 审计修复：公开声明 zombie 检查实际收集 capability header 符号，并将 header-inline 定义视为有效实现；缺失 `.cpp` 定义重新成为阻断项。
15. [x] D0 审计覆盖：将 split lobby handler 共享 helper 的公开头纳入声明/定义审计，防止跨 TU 接口缺失实现未被阻断。
16. [x] D0 审计覆盖：将 Store/Runtime locator 的公开能力头纳入声明/定义审计，防止 locator 生命周期接口与实现漂移。
17. [x] D0 审计覆盖：将 reconnect network orchestration 的公开能力头纳入声明/定义审计，防止异步 generation gate 接口与实现漂移。
18. [x] D2 首切片：queued lobby state 以 `compose_queued_lobby_state_apply_plan()` 计算 `state`、`game_state` 与 `launch_phase`，再由 `apply_queued_lobby_state_apply_plan()` 作为唯一 Local 写入入口；rich presence、peer push 和 generation-gated shared publish 顺序保持。
19. [x] D2 切片：`GBE_MarkDotaLaunchPhase()` 通过 `advance_launch_phase()` 单一化 monotonic `launch_phase` 更新；publish 和诊断仍在 coordinator 中按原顺序执行。
20. [x] D2 切片：generic lobby capture 以 `compose_generic_lobby_state_capture_plan()` 分别决定 `state` 与 `game_state` 写许可，再由 `apply_generic_lobby_state_capture_plan()` 应用；custom-game launch 回退保护和 stale-state 日志保持。
21. [x] D2 切片：shared restore 以 `compose_shared_lobby_runtime_restore_plan()` 计算 `state`、`game_state` 与 `launch_phase` 的字段组更新，再由 `apply_shared_lobby_runtime_restore_plan()` 应用；shared runtime 覆盖与 custom-game READYUP 回退拒绝保持。
22. [x] D2 切片：steam-auth ack 以 `compose_steam_auth_ack_launch_plan()` 计算 CRC、sequence 和 ack 标记，再由 `apply_steam_auth_ack_launch_plan()` 一次写入；既有 ticket CRC 派生、shared publish 与 synthetic push 顺序保持。
23. [x] D2 切片：4511 LAN server available 通过 `mark_launch_4511_seen()` 单一化幂等去重标记；首次标记才 publish shared state，重复通知不 re-publish。
24. [x] D2 切片：lifecycle `LobbyStateApply` 通过 `apply_lifecycle_lobby_state()` 单一化 `state` / `game_state` 字段组写入；executor 继续承担 action 序列、条件 gate 与副作用。
25. [x] D2 切片：shared restore 通过 `restore_launch_4511_seen()` 单一化 4511 去重标记复制；Coordinator 继续聚合字段变更以保留 client restore 语义。
26. [x] D2 切片：lifecycle `PostGameLobbyStateApply` 通过 `apply_postgame_lobby_state_plan()` 单一化 state、chat 与 cache 清理字段组；executor 继续承担 action 序列与副作用。
27. [x] D2 切片：shared restore 通过 `restore_lobby_connect()` 与 `restore_lobby_match_id()` 单一化 connect/match_id 复制；Coordinator 继续聚合字段变更以保留 client restore 语义。
28. [x] D2 切片：shared restore 通过 `restore_lobby_game_start_time()` 与 `restore_lobby_room_name()` 单一化 start_time/room 复制；零 start_time 拒绝与 client restore 聚合保持。
29. [x] D2 切片：shared restore 通过 `restore_lobby_owner_connected()`、`restore_lobby_owner_team()` 与 `restore_lobby_owner_slot()` 单一化 owner 字段复制；Coordinator 继续聚合字段变更。
30. [x] D2 切片：shared restore 通过 `compose_shared_lobby_options_restore_plan()` 与 `apply_shared_lobby_options_restore_plan()` 单一化 options/bot 字段组复制；Coordinator 继续聚合字段变更。
31. [x] D2 切片：shared restore 通过 `compose_shared_lobby_cache_restore_plan()` 与 `apply_shared_lobby_cache_restore_plan()` 单一化 cache 字段组复制；Coordinator 继续聚合字段变更。
32. [x] D2 切片：shared restore 通过 `restore_lobby_custom_game()` 单一化 custom_game 复制；比较复用 `custom_game_details_equal()`，Coordinator 继续聚合字段变更。
33. [x] D2 切片：shared restore 通过 `restore_lobby_generation()` 与 `restore_lobby_generic_lobby_id()` 单一化 identity 字段复制；generation counter 同步仍由 Coordinator 负责。
34. [x] D2 切片：generic capture 通过 `compose_generic_lobby_runtime_identity_capture_plan()` 与 `apply_generic_lobby_runtime_identity_capture_plan()` 单一化 room/match/server/connect/start_time 写入；launched LAN runtime 保护保持。
35. [x] D2 切片：generic capture 通过 `compose_generic_lobby_options_capture_plan()` 与 `apply_generic_lobby_options_capture_plan()` 单一化 options/bot 写入；空 raw key 跳过语义保持。
36. [x] D2 切片：generic capture 通过 `compose_generic_lobby_custom_game_capture_plan()` 与 `apply_generic_lobby_custom_game_capture_plan()` 单一化 custom_game 字段写入；空 raw key 跳过语义保持。

### 验收

- [ ] 五条核心路径具有唯一状态转换入口。
- [ ] 所有延迟任务均验证 lobby id 与 generation。
- [ ] state machine 的 transition result 与实际 runtime mutation 一一对应。
- [ ] 真实协议样本与离线回放通过。

### R4.1 验证记录

- `gbe_dota_lifecycle_state_machine_test` 通过。
- `bash tools/run_gc_verification.sh --full` 完整通过：audit、replay、flow、payload helper `555/555`、handler `90/90` 和 behavior replay 均通过。
- R4.1 时架构审计曾保留内部头 inventory 注记与 `Dependencies` 构造依赖识别；二者已分别在 R3 与 R2 审计收尾中消除。
- R4.2 更新 handler side-effect seam 基线：7041 publish 已迁至 executor，lifecycle handler 的直接 publish 计数从 2 降至 1。
- R4.2 后再次运行 `bash tools/run_gc_verification.sh --full`：handler seam 与 architecture boundary 均为 0；后续 R2/R3 审计收尾将公开依赖契约和 capability header 符号审计同步至当前实现。
- R4.3 的初版错误地复用了 finalize teardown gate；handler smoke 证明 8246 无 pending teardown，改为 immediate runtime-clear boundary 后 `bash tools/run_gc_offline_tests.sh --full` 通过，包含 handler `90/90`。
- R4.4 更新 handler side-effect seam 基线：7004 迁移后 lifecycle handler 的直接 shared publish 归零，直接 details update 从 2 降至 1。
- R4.5 审计确认 abandon、normal signout 和 reset 三类 deferred slot 在消费时均校验 lobby id 与 generation；新增 flow 回归验证 7035 deferred reset 的 action 类型、reason、generation boundary 及清理选项。
- R4.6 新增 flow 回归验证 7004 followup 的 details update、25 push 和 pending-finalize action 顺序；handler 直接 details update 归零。
- D2 queued launch/runtime apply 新增 lobby-state 回归，验证三字段作为一个 field group 写入；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 monotonic phase apply 新增 lobby-state 回归，验证推进与回退拒绝；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 generic capture 新增 lobby-state 回归，验证 custom-game launch 中 stale state/game_state 保留与 fresh 字段应用；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 shared runtime restore 新增 lobby-state 回归，验证 custom-game READYUP state 回退拒绝、shared game_state 与 launch_phase 覆盖；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 steam-auth 元数据新增 lobby-state 回归，验证派生 CRC、默认 sequence、ack 标记与既有值保留；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 4511 去重标记新增 lobby-state 回归，并保留 handler smoke 对“标记后 publish、重复不 re-publish”的序列验证；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 lifecycle state apply 新增 lobby-state 回归，验证 `state` 与 `game_state` 作为字段组写入；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 shared 4511 restore 新增 lobby-state 回归，验证 matching restore 不变更、不同值 restore 返回变更并更新标记；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 postgame state apply 新增 lobby-state 回归，验证 state、chat channel 与 cache 清理作为字段组写入；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 shared connect/match restore 新增 lobby-state 回归，验证空 connect/零 match 拒绝、匹配值无变更、不同值返回变更；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 shared start_time/room restore 新增 lobby-state 回归，验证零 start_time 拒绝、匹配值无变更、不同值返回变更；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。
- D2 shared owner restore 新增 lobby-state 回归，验证 owner_connected/team/slot 匹配值无变更、不同值返回变更；`bash tools/run_gc_verification.sh --full` 完整通过，含 audit、payload helper `555/555` 与 handler `90/90`。

## R5：持续验证与上游同步

### 目标

将现有离线测试、生产编译与协议捕获样本组成持续维护门槛。

### 实施步骤

1. 在 CI 中完成完整 `tools/run_gc_offline_tests.sh`，避免受交互开发时限影响。
2. 增加 Linux 与 Windows 生产构建任务。
3. 为每项协议更新保留最小原始 request/response fixture，覆盖 direct、wrapped、未知字段和字段缺失。
4. 每次同步上游后更新基线，记录冲突文件、处理策略和回归结果。

### 验收

- [ ] 合并请求同时运行离线测试和目标平台编译。
- [ ] 高风险入口具备真实或可复现的协议 fixture。
- [ ] 上游同步结果可追溯到具体冲突和验证命令。
