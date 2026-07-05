# GC 重构后续深化任务清单

## 执行规则

- 每次只处理一个 stage 的一个必要任务或一个小型增强任务。
- 生产逻辑、测试补强、文档治理可以在同一 stage 内完成，但提交前必须能用清单说明边界。
- 每个任务完成后记录实际验证命令。
- 涉及 header 声明迁移、registry 迁移、seam 扩展或审计规则时运行 `tools/run_gc_verification.sh`。
- 当前环境缺少 Premake 时记录环境限制，具备工具环境后补 `premake5 --with-gc-tests gmake2`。

## 推荐启动顺序

1. 先执行 Stage 0，建立下一轮基线和外部验证缺口。
2. 再执行 Stage 4.1，封装 host showcase flag；该任务范围小、调用点少、适合作为下一轮代码变更起点。
3. 然后执行 Stage 3，推进 `EquipItemsPlan` executor 化；该任务架构收益高，必须依赖现有顺序测试保护。
4. Stage 1、2、5、6 可按审查容量穿插执行，每次只迁一个声明组、一个 seam 或一小批 registry entries。

## Stage 0：生产构建和运行时验证基线

- [x] 0.1 记录当前验证基线
  - 运行 `tools/run_gc_verification.sh`
  - 记录 handler smoke、payload helper、audit 和 style gate 结果
  - 检查 `git status --short --branch`
  - 结果：`tools/run_gc_verification.sh` 通过；full offline tests 通过；payload helper tests `167/167` 通过；handler smoke tests `36/36` 通过；audit 无 zombie declarations、under-exposed definitions、dispatch mismatch 或 template ownership issue；`git diff --check` 通过
  - 状态：验证前 `git status --short --branch` 为 `## trae/agent-inRF11...origin/trae/agent-inRF11`，工作区干净
- [x] 0.2 补 Premake gate 验证记录
  - 在具备工具环境运行 `premake5 --with-gc-tests gmake2`
  - 验证 GC test target 可生成
  - 当前环境无工具时记录限制和后续执行命令
  - 环境检查：`command -v premake5` 无输出；`command -v ./premake5` 无输出
  - 结果：当前环境无法执行 Premake gate；具备工具环境后运行 `premake5 --with-gc-tests gmake2`，并验证 `tool_gbe_dota_gc_payload_helpers_test` 与 `tool_gbe_dota_handler_test` 生成
- [x] 0.3 建立真实路径验证清单
  - 覆盖 `7035 abandon current game`
  - 覆盖 `7004 signout`
  - 覆盖 `8052/8053 custom game loading`
  - 覆盖 `2569 equip items full forward`
  - 覆盖 reconnect/direct-connect flow
  - 结果：新增 `runtime-validation-checklist.md`，记录每条真实路径的目标、offline coverage、live validation steps 和 pending live validation 状态

## Stage 1：Internal header 继续收缩

- [x] 1.1 盘点 `gbe_dota_gc_internal.h` 剩余声明分组
  - 分类 wire payload、lobby payload、template/replay、logging、stateful orchestration
  - 输出保留、迁移、下沉清单
  - 结果：新增 `internal-header-declaration-inventory.md`，将剩余声明分为 logging/trace、wire payload and proto patch、lobby payload and practice lobby builders、template/replay ownership、shared tables/serializer utilities、shared mutable state/stateful orchestration、down-sink candidates；建议 Stage 1.2 新建 `gbe_dota_payload_wire_helpers.h` 迁移 wire/hello 声明，Stage 1.3 新建 `gbe_dota_payload_lobby_helpers.h` 迁移 lobby payload 声明，Stage 1.4 再评估 template/replay 常量和 peripheral builders
  - 验证：`git diff --check` 通过
- [x] 1.2 迁移 wire payload declarations
  - 新建或复用窄 header 承载 DTO parser、proto patch、template patch 声明
  - 更新生产和测试 include
  - 验证 `tools/run_gc_verification.sh`
  - 结果：新增 `dll/gbe_dota_payload_wire_helpers.h`，迁出 direct proto account id patch、template id patch、direct replay message、lobby template id/cache owner patch、welcome body、hello/server hello context、hello extraction 和 client/server welcome builder 声明；`gbe_dota_gc_internal.h` 过渡 include 新窄 header 并保留 server hello cache accessor/state 声明；生产调用方和 payload helper test 显式 include 新 header
  - 验证：`tools/run_gc_verification.sh` 通过；handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过；audit 显示 header declarations 从 `57` 收缩到 `40`，无 zombie declarations、under-exposed definitions、dispatch mismatch、template ownership、source-list inclusion、side-effect seam 或 reason inventory issue；`git diff --check` 通过
- [x] 1.3 迁移 lobby payload declarations
  - 新建或复用窄 header 承载 lobby cache/details/launch/chat payload 声明
  - 避免同时移动 template/replay owner
  - 验证 `tools/run_gc_verification.sh`
  - 结果：新增 `dll/gbe_dota_payload_lobby_helpers.h`，迁出 chat channel response、custom game display name、practice lobby 26/cache subscribed/details update builders、lobby SO object compose、top custom games list 和 other-left-channel parser 声明；`gbe_dota_gc_internal.h` 过渡 include 新窄 header 并保留 template/replay constants；生产调用方、wire helper TU 和 payload helper test 显式 include 新 header；`tools/_audit_gc_refactor.py` 将 item/lobby/wire narrow headers 纳入 public declaration set
  - 验证：`tools/run_gc_verification.sh` 通过；handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过；audit 显示 internal header declarations 从 `40` 收缩到 `29`，无 zombie declarations、under-exposed definitions、dispatch mismatch、template ownership、source-list inclusion、side-effect seam 或 reason inventory issue；`git diff --check` 通过
- [x] 1.4 评估 template/replay declarations
  - 保持大型 canned payload owner 不变
  - 仅在调用边界明确时新增窄 header
  - 验证 audit template ownership 仍通过
  - 结果：更新 `internal-header-declaration-inventory.md`，明确 `GBE_kDotaAbandonPersonaStateInitHex`、`GBE_kDotaOfficial032PracticeLobby26Hex`、`GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex`、`GBE_kDotaPracticeLobbyCacheSubscribedTemplate`、`GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage`、`GBE_PrepareDotaPersonaStatePeripheralMessage` 暂留 `gbe_dota_gc_internal.h`；原因是仍跨 chat abandon、launch flow、launch fallback、lobby snapshot replay 和 lobby payload builder 路径共享，当前创建 template/replay public header 收益不足
  - 验证：`python3 tools/_audit_gc_refactor.py` 通过，template/replay canned blob ownership issues 为 `0`；`git diff --check` 通过

## Stage 2：Dependency seam 扩展

- [x] 2.1 封装 server GC forward seam
  - 首选 inventory equip full forward 路径
  - 保持 full item cache 在 create/update 前
  - 增加或更新顺序测试
  - 结果：新增 `GBE_ForwardDotaEquipItemsToServerGC` seam，将 `2569` equip full forward 的 full CacheSubscribed、per-item emsg `21` 和 emsg `26` server GC 推送从 handler 主流程封装到成员方法；handler 继续按 plan 串行执行 local response、save、server GC forward、network broadcast 和 snapshot refresh；现有 `test_inventory_equip_full_forward` 顺序测试继续覆盖 full cache 早于 emsg `21/26`
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch、template ownership、source-list inclusion、side-effect seam 或 reason inventory issue，`git diff --check` 通过
- [x] 2.2 封装 network broadcast seam
  - 保留 business reason
  - 保持 local response、save、server GC forward 之后的当前顺序
  - 增加或更新顺序测试
  - 结果：新增 `GBE_BroadcastDotaEquippedItemsToGameServers` seam，将 `2569` equip full forward 路径里的 equipped item inventory response 网络广播从 handler 主流程封装到成员方法；handler 仍在 server GC forward 之后调用该 seam，并由现有 `test_inventory_equip_full_forward` 断言 `NetworkBroadcast` 位于 server GC emsg `26` 之后、`LobbySnapshotRefresh` 之前
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch、template ownership、source-list inclusion、side-effect seam 或 reason inventory issue，`git diff --check` 通过
- [x] 2.3 封装 lobby publish/details update seam
  - 覆盖 lobby publish 或 practice lobby details update 中一个低风险路径
  - 保留 wrapped/session/reason 语义
  - 结果：新增 `GBE_PublishDotaPracticeLobbySetDetailsUpdate` seam，封装 `7046` set-details 路径中的 local member publish、shared lobby publish、metadata publish 和 practice lobby details update；handler 保留字段 mutation、custom game normalization 和 slot normalization 后调用 seam；现有 `test_lobby_set_details_mutates_before_publish_and_details_update` 继续覆盖 mutation 先于 publish/details update，且 wrapped/session/reason 语义保持不变
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch、template ownership、source-list inclusion、side-effect seam 或 reason inventory issue，`git diff --check` 通过
- [x] 2.4 封装 snapshot refresh seam
  - 优先处理 equip snapshot refresh
  - 保持 snapshot refresh 在网络广播后的当前顺序
  - 结果：新增 `GBE_RefreshDotaEquipLobbySnapshot` seam，封装 equip 路径中的 private lobby snapshot replay flag clear 和 current private lobby snapshot replay；handler 仍在 network broadcast 之后调用该 seam；现有 `test_inventory_equip_full_forward` 继续断言 `LobbySnapshotRefresh` 位于 `NetworkBroadcast` 之后
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch、template ownership、source-list inclusion、side-effect seam 或 reason inventory issue，`git diff --check` 通过

## Stage 3：Equip action plan executor

- [x] 3.1 定义 execution context
  - 收集 source job、local steam id、server GC availability、snapshot availability 等 executor 输入
  - context 不持有可跨 action 失效的引用
  - 结果：新增 `EquipItemsExecutionContext`，以标量保存 `has_source_job`、`source_job`、`local_steam_id`，并保存本次执行时解析出的 `server_gc` 指针；snapshot availability 仍由 planner 在 `snapshot_refresh_reason` 中固化，executor 只消费 plan 输出
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 全绿，`git diff --check` 通过
- [x] 3.2 提取 `execute_equip_items_plan`
  - 放在 `gbe_dota_inventory_handlers.cpp` anonymous namespace
  - executor 负责 SO update、local response、save、server GC forward、network broadcast、snapshot refresh
  - planner 保持 pure helper 边界
  - 结果：在 `gbe_dota_inventory_handlers.cpp` anonymous namespace 新增 `execute_equip_items_plan`，集中执行 SO update、local `2570` response、save、server GC forward、network broadcast 和 snapshot refresh；新增 `build_equip_response_message`，handler 负责生成 update/response payload、提交 mutated items，然后调用 executor；planner 仍只产出 `EquipItemsPlan` 和有序 action list，不直接调用 coordinator/network/file/server GC/lobby snapshot 副作用
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 全绿，`git diff --check` 通过
- [x] 3.3 扩展 equip executor focused tests
  - 覆盖 save、server GC forward、network broadcast、snapshot refresh 顺序
  - 覆盖 missing item 和 empty body 不执行 executor 副作用
  - 结果：复用既有 focused coverage：`test_inventory_equip_full_forward` 覆盖 SO update、local response、save、server GC full cache、emsg `21/26`、network broadcast、snapshot refresh 顺序；`test_inventory_equip_empty` 覆盖 parse failure 不执行副作用；`test_inventory_equip_planner_missing_item` 覆盖 missing item 只规划 response+save；当前测试已覆盖 executor 化的关键风险，无需新增重复测试
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 全绿，`git diff --check` 通过
- [x] 3.4 评估推广条件
  - 记录 unlock/set style 是否适合 action plan 模式
  - 不满足复用条件时保持局部 executor
  - 结果：暂不推广到 unlock/set style；`2571` unlock style 有 early invalid-style return、consumable destroy 和 response 三段紧耦合副作用，`2577` set style 有 callback、server GC cache push、save、response 的较短直线流程；当前抽成通用 action plan 会增加模板/adapter 复杂度，收益低于局部代码清晰度。后续仅在多个 inventory handlers 共享同类 action 序列时再推广
  - 验证：`git diff --check` 通过

## Stage 4：Runtime/global state 继续封装

- [x] 4.1 封装 host showcase flag
  - 新增 get/set/clear accessor
  - 保持 `7034` showcase repush guard 行为
  - 增加 focused test 或 audit coverage
  - 结果：新增 `GBE_HasPushedDotaHostShowcaseEquip`、`GBE_MarkDotaHostShowcaseEquipPushed`、`GBE_ClearDotaHostShowcaseEquipPushed`；`7034` showcase repush guard 改为通过 accessor 读写；handler smoke test 新增 `test_match_7034_host_showcase_repush_guard_marks_once`
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch 或 template ownership issue，`git diff --check` 通过
- [x] 4.2 封装 server hello cache
  - 新增 get/set/clear accessor
  - 保持 direct server hello parse 和 welcome replay 行为
  - 验证 payload helper/welcome coordinator 仍通过
  - 结果：新增 `GBE_HasLastDotaServerHelloContext`、`GBE_GetLastDotaServerHelloContext`、`GBE_SetLastDotaServerHelloContext`、`GBE_ClearLastDotaServerHelloContext`；direct ServerHello parse 改为通过 setter 缓存，cached ServerWelcome replay 改为通过 getter 读取
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch 或 template ownership issue，`git diff --check` 通过
- [x] 4.3 封装 launch/sync flags
  - 覆盖 login sync sent、private lobby snapshot replayed、launch state pushed game state、persona signature
  - 每次只迁移一个小状态组
  - 部分结果：完成 `GBE_dota_login_sync_sent` 小组封装，新增 `GBE_HasSentDotaLoginSync`、`GBE_MarkDotaLoginSyncSent`、`GBE_ClearDotaLoginSyncSent`；welcome coordinator 和 shutdown clear 改为通过 accessor 读写
  - 部分结果：完成 `GBE_dota_private_lobby_snapshot_replayed` 小组封装，新增 `GBE_HasReplayedDotaPrivateLobbySnapshot`、`GBE_MarkDotaPrivateLobbySnapshotReplayed`、`GBE_ClearDotaPrivateLobbySnapshotReplayed`；inventory snapshot refresh、lobby snapshot replay 和 member-change reset 改为通过 accessor 读写
  - 部分结果：完成 `GBE_last_dota_launch_state_pushed_game_state` 小组封装，新增 `GBE_GetLastDotaLaunchStatePushedGameState`、`GBE_SetLastDotaLaunchStatePushedGameState`、`GBE_ClearLastDotaLaunchStatePushedGameState`；launch-state duplicate guard 和 runtime/postgame reset 改为通过 accessor 读写
  - 部分结果：完成 launch persona 和 direct-connect callback signature 小组封装，新增 get/set/clear accessor；rich presence clear、direct-connect duplicate guard、persona duplicate guard 和 member-change reset 改为通过 accessor 读写
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch 或 template ownership issue，`git diff --check` 通过
- [x] 4.4 评估小型 runtime storage struct
  - 仅承载已通过 accessor 稳定的状态组
  - 保持 accessor API 不变
  - 不迁移 `GBE_local_lobby` 和 `GBE_shared_dota_lobby_state`
  - 结果：新增 `runtime-state-storage-evaluation.md`；结论为暂缓创建新 storage struct，保留已稳定 accessor，等待后续功能重构阶段产生 grouped reset 或 diagnostics 的实际收益后再迁移
  - 验证：`git diff --check` 通过

## Stage 5：Post-login registry 继续收口

- [x] 5.1 选择下一批 simple direct-only handlers
  - 候选 profile/card、emoticon、conduct、coaching summary
  - 排除 template replay、launch chain 和复杂 fallback 路径
  - 结果：新增 `post-login-registry-candidates.md`；选择 `7534` profile card、`2581` lookup account name、`7503` emoticon data、`8095` conduct scorecard、`8800` coaching summary 作为 Stage 5.2 首批 direct-only registry entries；明确推迟 template replay、launch chain、lobby lifecycle、inventory mutation、cache refresh、`7034` runtime state 和复杂 chat/lobby broadcast 路径
  - 验证：`git diff --check` 通过
- [x] 5.2 迁移少量 registry entries
  - 保留 direct/wrapped path guard
  - 保留 source job、target job、session 语义
  - 结果：将 `7534` profile card、`2581` lookup account name、`7503` emoticon data、`8095` conduct scorecard、`8800` coaching summary 加入 `GBE_DispatchDotaPostLoginRequest` direct-only registry；保留 `DotaGcRequestPath::Direct` guard，沿用 `body/body_size/has_request_job/request_job_id` 语义；删除对应 explicit direct branches
  - 验证：`tools/run_gc_verification.sh` 通过，audit 显示 `All 24 dispatch entries map to the expected handlers`
- [x] 5.3 扩展 dispatch audit
  - 更新 `_audit_gc_refactor.py` expected mapping
  - 覆盖新 adapter 的 path guard
  - 结果：`POST_LOGIN_DIRECT_DISPATCH_ENTRIES` 增加 5 个新 direct-only adapter；既有 direct guard 正则覆盖新 adapter 的 `DotaGcRequestPath::Direct` 校验
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 无 zombie declarations、under-exposed definitions、dispatch mismatch 或 template ownership issue，`git diff --check` 通过
- [x] 5.4 运行 full verification
  - `tools/run_gc_verification.sh`
  - 记录迁移范围和保留显式路径原因
  - 结果：full verification 已覆盖 Stage 5.2/5.3 registry 迁移；迁移范围为 5 个 simple direct-only handlers，显式路径继续保留 template replay fallback、launch chain、lobby lifecycle、inventory mutation、cache refresh、`7034` runtime state 和复杂 chat/lobby broadcast 路径
  - 验证：`tools/run_gc_verification.sh` 通过，handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，audit 显示 `All 24 dispatch entries map to the expected handlers` 且无 zombie declarations、under-exposed definitions、dispatch mismatch 或 template ownership issue，`git diff --check` 通过

## Stage 6：Audit 和验证规则强化

- [x] 6.1 增加 source-list inclusion audit
  - 检查新增 GC `.cpp` 是否进入 shell test source list 或记录豁免
  - 检查可选 Premake test target source list 是否同步
  - 结果：`tools/_audit_gc_refactor.py` 新增 `AUDIT 6: GC source-list inclusion`；枚举 `dll/gbe_dota_*.cpp`，确认 testable split GC TUs 出现在 `tools/run_gc_offline_tests.sh` 或 `premake5.lua` GC test source lists 中，production-only 或 wrapper-compiled TUs 必须在 `SOURCE_LIST_AUDIT_EXEMPTIONS` 中显式说明原因
  - 验证：`tools/run_gc_verification.sh` 通过；audit 显示 `All 8 testable split GC TUs appear in shell or Premake test source lists`、`19 production-only or wrapper-compiled TUs have explicit audit exemptions`、`Source-list inclusion issues: 0`；handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，`git diff --check` 通过
- [x] 6.2 增加 side-effect seam audit
  - 对普通 handler 文件中直接调用高风险副作用 API 做提示或失败
  - 允许明确 owner 文件和 executor seam
  - 结果：`tools/_audit_gc_refactor.py` 新增 `AUDIT 7: Handler side-effect seam drift`；对普通 `gbe_dota_*_handlers.cpp` 中的文件保存、server GC forward、network broadcast、lobby publish、snapshot refresh 等高风险副作用调用建立显式 baseline；新增或计数漂移的调用会失败并要求迁入 approved seam 或更新 baseline 原因
  - 验证：`tools/run_gc_verification.sh` 通过；audit 显示 `All 48 high-risk handler side-effect calls match 18 explicit baseline entries`、`Handler side-effect seam issues: 0`；handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，`git diff --check` 通过
- [x] 6.3 增加 reason inventory audit
  - 检查高风险 reason string 是否出现在治理文档或测试中
  - 避免新增临时 reason 漂移
  - 结果：`tools/_audit_gc_refactor.py` 新增 `AUDIT 8: High-risk reason inventory`；对 14 个高风险 reason string 检查 `.monkeycode/specs/gc-refactor-next-steps/reason-trace-governance.md` inventory 记录，以及测试/规格覆盖文本，避免后续 reason 改名或新增临时 reason 脱离治理
  - 验证：`tools/run_gc_verification.sh` 通过；audit 显示 `All 14 high-risk reason strings are documented and covered`、`High-risk reason inventory issues: 0`；handler smoke tests `37/37` 通过，payload helper tests `167/167` 通过，`git diff --check` 通过
- [x] 6.4 修正 payload helper test 计数显示
  - 当前输出存在 `[1/25]` 和实际 `26/26` 小不一致
  - 修正文案并保持测试语义不变
  - 结果：`tools/gbe_dota_gc_payload_helpers_test/gbe_dota_gc_payload_helpers_test.cpp` 的测试进度标签统一为 `[1/26]` 到 `[26/26]`，只修改输出文案，测试语义保持不变
  - 验证：`tools/run_gc_verification.sh` 通过；payload helper test 输出显示 `[1/26]` 到 `[26/26]`，`Results: 167/167 passed, 0 failed`；handler smoke tests `37/37` 通过；audit 无 zombie declarations、under-exposed definitions、dispatch mismatch、template ownership、source-list inclusion、side-effect seam 或 reason inventory issue；`git diff --check` 通过

## Stage 7：交付检查点

- [x] 7.1 运行完整验证
  - `tools/run_gc_verification.sh`
  - `bash -n tools/run_gc_verification.sh`
  - `bash -n tools/run_gc_offline_tests.sh`
  - 验证：`tools/run_gc_verification.sh` 通过；`bash -n tools/run_gc_verification.sh` 通过；`bash -n tools/run_gc_offline_tests.sh` 通过
- [x] 7.2 检查文档索引
  - 确认 `.monkeycode/docs/INDEX.md` 引用的规格文件存在
  - 确认无断链目录
  - 验证：索引中的 markdown 引用检查通过，输出 `all indexed markdown files exist`
- [x] 7.3 提交前审查
  - 查看 `git diff --stat`
  - 查看 `git status --short --branch --untracked-files=all`
  - 记录无法执行的 Premake 或真实客户端验证限制
  - 状态：`git status --short --branch --untracked-files=all` 显示当前分支 `trae/agent-inRF11...origin/trae/agent-inRF11`，存在本轮 Stage 0/4/5/6/7 代码与文档改动，以及 3 个新增 follow-up 文档
  - Diff：`git diff --stat` 显示 17 个已跟踪文件变更，约 `567 insertions(+), 106 deletions(-)`，另有新增文档 `post-login-registry-candidates.md`、`runtime-state-storage-evaluation.md`、`runtime-validation-checklist.md`
  - Style：`git diff --check` 通过
  - 限制：当前环境 `command -v premake5` 和 `command -v ./premake5` 无输出，仍无法执行 `premake5 --with-gc-tests gmake2`；真实客户端路径保持 `runtime-validation-checklist.md` 中的 pending live validation 状态
