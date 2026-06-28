# steam_game_coordinator.cpp 重构进度与任务列表

> 本文件记录对 `dll/steam_game_coordinator.cpp`(原始 15029 行的"巨石"文件)进行分阶段拆分重构的完整进度与后续计划,便于会话切换后快速恢复上下文。
>
> **当前分支**: `trae/agent-inRF11`
> **最后提交**: `f0fbea7` — refactor(gc): delete dead code and prep shared symbols for lobby-flow split (phase 2.4a)

---

## 1. 重构目标

将原本集中在一个 `.cpp` 中的 ~15000 行 GC 代码按职责拆分到多个翻译单元(TU),使每个 TU 聚焦单一关注点,降低耦合、便于独立阅读与单元测试,同时保持行为完全不变(纯结构化重构)。

**核心约束**:
- 行为零变化:仅做符号可见性调整、文件拆分、删除已验证的死代码,不改逻辑。
- `Steam_Game_Coordinator` 类成员函数仍归属于该类,只是物理上分布到多个 TU;通过内部头文件 `gbe_dota_gc_internal.h` 共享跨 TU 调用的符号。
- 拆分策略遵循"先分析耦合 → 提升可见性 → 移动定义"的三步法,每个阶段提交独立、可回滚。
- 所有结构性改动配套 Python 脚本(`tools/_phaseNN*.py`)记录操作过程,便于审阅与复现。

---

## 2. 已完成阶段

### Phase 1.x — 小规模清理与试点拆分(2026-06-27 前后)

| 阶段 | 提交摘要 | 说明 |
|---|---|---|
| 1.2e | refactor(gc): extract unlock/set item-style handlers | 将 unlock/set item-style handler 拆出到独立 TU |
| 1.4 | refactor(gc): remove dead code already handled by shared dispatch | 删除已被 shared dispatch 覆盖的死代码 |

### Phase 2.2 — 拆分 lobby-state 成员(2026-06-27)

提交:`c579f7d` refactor(gc): split lobby-state members to gbe_dota_lobby_state_coordinator.cpp (phase 2.2)

- 新增 [dll/gbe_dota_lobby_state_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_state_coordinator.cpp)(1701 行)
- 后续修复:`f038ccd`、`7375c06`(共享 launch-phase enum 与 shared lobby state 跨 TU)

### Phase 2.3a — 外部化 List B static 符号(2026-06-28)

提交:`9d87872` refactor(gc): externalize 26 shared static symbols for handler split (phase 2.3a)

- 脚本:[tools/_phase23a_externalize.py](file:///workspace/tools/_phase23a_externalize.py)
- 将 26 个被 handler 代码和非 handler 代码同时引用的 file-scope `static` 符号改为外部链接(`static const` → `extern const`,`static` → 移除前缀)
- 在 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h) 添加对应 extern 声明
- 把 `ser_var` 模板从 .cpp 移到 header(以 `inline` 形式)
- 耦合分析报告:[tools/_phase23_coupling_report.txt](file:///workspace/tools/_phase23_coupling_report.txt)

### Phase 2.3b — 提取 62 个 GBE_HandleDota* handler + 23 个 List A static 符号(2026-06-28)

提交:`0c5f2f1` refactor(gc): split dota handlers to gbe_dota_handlers.cpp (phase 2.3b)

- 脚本:[tools/_phase23b_extract_handlers.py](file:///workspace/tools/_phase23b_extract_handlers.py)
- 新增 [dll/gbe_dota_handlers.cpp](file:///workspace/dll/gbe_dota_handlers.cpp)(6335 行)
- 移走 62 个 `GBE_HandleDota*` 成员函数
- 移走 23 个仅被 handler 使用的 List A static 符号(保持 `static` 关键字,作为新 TU 的内部符号)
- 后续修复:`7815218`(为 handler TU 补充类型别名与 `GBE_ProtoField`)

### Phase 2.4a — 删除死代码 + 为 Phase 2.4b 预共享符号(2026-06-28,当前 HEAD)

提交:`f0fbea7` refactor(gc): delete dead code and prep shared symbols for lobby-flow split (phase 2.4a)

- 脚本:[tools/_phase24a_delete_dead.py](file:///workspace/tools/_phase24a_delete_dead.py)
- 删除 18 个未引用的死代码 static 符号(5 函数 + 13 变量,共 ~443 行)
- 将 `GBE_DotaServerHelloContext` 结构体从 .cpp 移到 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h)
- 外部化 `GBE_BuildDirectDotaServerWelcome`(函数)与 `GBE_last_dota_server_hello_context`(变量)— 这两个是 "core GC code (stays)" 与 "lobby-flow code (will move)" 之间唯一需要共享的符号
- 主文件:`8862 → 8398` 行

---

## 3. 当前文件布局

> 以下反映 Phase 2.4b 执行后(尚未提交)的状态。

| 文件 | 行数 | 职责 |
|---|---:|---|
| [dll/steam_game_coordinator.cpp](file:///workspace/dll/steam_game_coordinator.cpp) | 7774 | GC 类核心:基础设施(SendMessage_/RetrieveMessage/network_callback/RunCallbacks)、lobby-state 状态机、welcome/cache/payload 构造、路由分发、`handle_dota_client_message` |
| [dll/gbe_dota_handlers.cpp](file:///workspace/dll/gbe_dota_handlers.cpp) | 6335 | 62 个 `GBE_HandleDota*` 请求 handler + 23 个 handler-only static 符号 |
| [dll/gbe_dota_lobby_flow_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_flow_coordinator.cpp) | 702 | 10 个 lobby-flow helper 成员函数 + 4 个 List X static 常量(Phase 2.4b 新增) |
| [dll/gbe_dota_lobby_state_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_state_coordinator.cpp) | 1701 | lobby-state 成员管理 |
| [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h) | 188 | 跨 TU 共享的内部头:外部链接符号声明、`GBE_DotaServerHelloContext` 结构、`ser_var` 模板 |
| **合计** | **16700** | |

> Phase 2.4b 累计变更:主文件 -624 行,新 TU +702 行(含 78 行 license/include 头)。原始主文件 15029 行 → 当前 7774 行,累计缩减 48%。

主文件 `steam_game_coordinator.cpp` 当前剩余的 `Steam_Game_Coordinator::` 成员函数布局(行号 → 函数):

```
L3222  GBE_DispatchDotaPostLoginRequest
L3297  gc_enabled / client_items / server_items / parse_gc_config / is_welcome_message
L3358  GBE_ApplyQueuedLobbyState
L3429  push_incoming
L3458  GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon
L3474  GBE_DiscardQueuedDotaLaunchMessagesForAbandon
L3527  GBE_ShouldSuppressDotaAbandonedLobby
L3532  GBE_MarkDotaAbandonedLobbySuppressed
L3563  GBE_ClearDotaAbandonedLobbySuppression
L3579  push_incoming_now
L3611  build_msg_header / parse_msg_header / build_protomsg_header / parse_protomsg
L3667  GBE_PatchDotaLoginCacheSubscribedInventory
L3950  item_id_local_to_network / item_id_network_to_local
L3984  item_to_gcstruct / item_to_gcprotobuf
L4084  handle_set_item_pos / handle_delete_item / handle_motd_request
       handle_respawn / handle_set_item_style / handle_adjust_equip_state
       handle_set_multiple_item_pos
L4261  callback_client_welcome / callback_server_welcome
L4292  GBE_MaybePrimeDotaServerWelcomeFromCache
L4399  callback_items_received / callback_items_removed
       callback_item_updated / callback_item_deleted / callback_respawn_request
L4647  steam_network_callback / steam_run_every_runcb
L4714  initialize_gc / shutdown_gc / on_appid_changed
L4824  load_items_from_file / save_items_to_file
L5012  set_item_pos / delete_item / request_user_items / find_items_request
       remove_user_items / on_client_connected / on_client_disconnected
L5230  GBE_PushDotaLoginSyncMessages
L5360  GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay (×2 overloads)
L5488  GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload (×2 overloads)
L5540  GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate
L5582  GBE_TryQueueDotaPrelaunch021
L5612  GBE_SetDotaLobbyMemberConnected / GBE_SetDotaLobbyMemberRuntimeState
L5660  GBE_ShouldHoldDotaLanLaunchForRemoteMembers
L5687  GBE_TryQueueDotaRuntimeLobbyDetailsUpdate
L5768  GBE_HasDotaLaunchServerSetupSync / GBE_MarkDotaLaunchPhase
       GBE_TryAdvanceDotaLaunchToRun
L5814  GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots
L5860  GBE_GetDotaGenericLobbySnapshots
L6031  GBE_GetDotaJoinableCustomLobbiesHTTPJSON
L6082  ResetGCMemory
L6138  GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot
L6247  GBE_PushDotaLaunchStateToClientPeer
L6388  GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed
L6462  GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate
L6542  GBE_GetDotaLobbyOwnerName
L6556  GBE_QueueDotaPostGameTeardown
L6662  GBE_SendDotaPracticeLobbyDetailsUpdate
L6724  GBE_PushDotaResponse
L6767  GBE_SendDotaCustomGameLaunchSetupFlow
L6816  handle_dota_client_message            ← 核心分发,留在主文件
─────────────────── Phase 2.4b 候选移走区 ───────────────────
L7018  GBE_ResetDotaPracticeLobbyLaunchPeripheralState
L7023  GBE_ShouldTrackDotaPracticeLobbyLateSteamChain
L7030  GBE_MaybeQueueDotaPracticeLobbySteamAuthAck
L7108  GBE_UpdateDotaPracticeLobbyLaunchRichPresence
L7194  GBE_ClearDotaPracticeLobbyLaunchRichPresence
L7217  GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback
L7348  GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState
L7422  GBE_ReapplyDotaPracticeLobbyLaunchRichPresence
L7534  GBE_FinalizeDotaAbandonAfterOtherLeftChannel
L7551  GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed
─────────────────── Phase 2.4b 候选移走区结束 ───────────────
L7635  SendMessage_                          ← 核心基础设施,留在主文件
L7705  IsMessageAvailable
L7742  RetrieveMessage
L7872  network_callback_inventory_request
L7935  network_callback_inventory_response
L8059  network_callback_item_update
L8115  network_callback_item_deletion
L8156  network_callback_respawn_request
L8169  network_callback
L8242  RunCallbacks
```

---

## 4. 待完成任务

### Phase 2.4b — 提取 lobby-flow helpers 到新 TU【进行中】

**目标**:将主文件 L7018–L7634 区间的 10 个 lobby-flow 辅助成员函数提取到新 TU(命名建议 `dll/gbe_dota_lobby_flow_coordinator.cpp`,与现有 `gbe_dota_lobby_state_coordinator.cpp` 命名风格一致)。

**候选移走的函数**(10 个):
1. `GBE_ResetDotaPracticeLobbyLaunchPeripheralState`
2. `GBE_ShouldTrackDotaPracticeLobbyLateSteamChain`
3. `GBE_MaybeQueueDotaPracticeLobbySteamAuthAck`
4. `GBE_UpdateDotaPracticeLobbyLaunchRichPresence`
5. `GBE_ClearDotaPracticeLobbyLaunchRichPresence`
6. `GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback`
7. `GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState`
8. `GBE_ReapplyDotaPracticeLobbyLaunchRichPresence`
9. `GBE_FinalizeDotaAbandonAfterOtherLeftChannel`
10. `GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed`

**留在主文件的边界函数**:
- `handle_dota_client_message` (L6816)— 核心分发器,所有 dota 请求的入口
- `SendMessage_` / `IsMessageAvailable` / `RetrieveMessage` / `network_callback_*` / `RunCallbacks` (L7635+)— GC 类对外接口与回调骨架

**分析结果摘要**(2026-06-28,[_phase24b_analyze.py](file:///workspace/tools/_phase24b_analyze.py) 执行后,详见 [tools/_phase24b_coupling_report.txt](file:///workspace/tools/_phase24b_coupling_report.txt)):

- 10 个目标函数共 606 行,边界已确认(L7018–L7632)
- **List X(可跟随移走的 static 符号):4 个** — 全是 `GBE_kDotaLaunchPersonaState*Hex` constexpr 字符串常量(L271/273/275/277),仅被 `GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState` 引用
- **List Y(需要 externalize 的 shared static 符号):0 个** — Phase 2.4a + 早期阶段的预备工作已完整覆盖跨 TU 依赖
- List Z(留在主文件):35 个 — 不被目标函数引用
- 已 externalize 且被目标函数引用的符号:5 个(`GBE_GC_DebugLog`、`GBE_kSteamTicketAuthComplete`、`GBE_PrepareDotaPersonaStatePeripheralMessage`、`GBE_kDotaAbandonPersonaStateInitHex`、`GBE_shared_dota_lobby_state`)— 确认预备工作充分
- 验证:`gbe_dota_handlers.cpp` 与 `gbe_dota_lobby_state_coordinator.cpp` 均不引用 List X 的 4 个符号,可安全跟随移走

**执行步骤**(参照 Phase 2.3a/2.3b 模式):

- [x] **2.4b-step1**:写 `tools/_phase24b_analyze.py`,产出 `tools/_phase24b_coupling_report.txt`(已完成)
- [~] **2.4b-step2**:跳过 — List Y 为空,无需 externalize 任何新符号
- [x] **2.4b-step3**:写 `tools/_phase24b_extract.py`,将 10 个函数 + 4 个 List X static 符号移到新 TU `dll/gbe_dota_lobby_flow_coordinator.cpp`(已完成,主文件 8398→7774 行,新 TU 702 行)
- [~] **2.4b-step4**:无需修改 — [premake5.lua](file:///workspace/premake5.lua) 的 `common_files` 用 `"dll/**"` 通配符自动收集 dll/ 下所有源文件,新 TU 会被自动包含(与 `gbe_dota_handlers.cpp` 同机制)
- [ ] **2.4b-step5**:提交 `refactor(gc): extract lobby-flow helpers to gbe_dota_lobby_flow_coordinator.cpp (phase 2.4b)`

**风险与缓解**:
- 当前沙箱无构建环境,无法本地验证编译。每次外部化/移动都基于 brace 跟踪 + word-boundary regex 严格校验,与 Phase 2.3a/2.3b/2.4a 同一手法,已在该项目验证可靠。
- 本次 List Y 为空,执行路径最简(仅移动定义,不改任何符号可见性),风险最低。

### Phase 2.5+ — 后续路线图(待规划,优先级递减)

主文件 8398 行仍然偏大,后续可继续按职责拆分。候选方向(需各自做耦合分析后再定):

- **2.5**:提取 lobby-state 构造/快照相关函数(`GBE_BuildCurrentDotaPracticeLobby*` / `GBE_BuildAuthoritativeDotaPracticeLobby*` / `GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots` / `GBE_GetDotaGenericLobbySnapshots` / `GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot`)— L5360–6247 区间
- **2.6**:提取 lobby launch/teardown 流程函数(`GBE_TryQueueDota*` / `GBE_MarkDotaLaunchPhase` / `GBE_TryAdvanceDotaLaunchToRun` / `GBE_QueueDotaPostGameTeardown` / `GBE_SendDota*` / `GBE_PushDotaResponse` / `GBE_PushDotaLaunchStateToClientPeer`)— L5582–6767 区间
- **2.7**:提取 inventory/item 相关函数(`item_id_*` / `item_to_*` / `handle_set_item_*` / `handle_delete_item` / `callback_items_*` / `load_items_from_file` / `save_items_to_file` / `set_item_pos` / `delete_item` / `find_items_request` / `remove_user_items`)— L3950–5230 区间
- **2.8**:提取 welcome/hello 相关函数(`GBE_PatchDotaLoginCacheSubscribedInventory` / `callback_client_welcome` / `callback_server_welcome` / `GBE_MaybePrimeDotaServerWelcomeFromCache` / `GBE_PushDotaLoginSyncMessages`)— L3667–5360 区间
- **2.9**:把主文件中剩余的纯函数 static helpers(`GBE_PatchDota*` / `GBE_ReplayDota*` / `GBE_AdaptDota*` / `GBE_ExtractDota*` / `GBE_PrepareDota*` / `GBE_BuildDirectDota*Welcome` / `GBE_ComposeDota*Welcome`)按主题归并到对应 TU,或单独建 `gbe_dota_gc_payload_helpers.cpp`

完成 2.5–2.9 后,主文件预期可缩减至 ~2000 行,仅保留 GC 类的核心基础设施(初始化、消息收发骨架、回调路由、网络回调入口)与 `handle_dota_client_message` 顶层分发。

---

## 5. 关键设计约定(沿用既有约定)

1. **行为零变化**:所有阶段仅做物理重组,不改逻辑、不改签名、不改 ABI。
2. **三步法**:每个拆分阶段遵循 ① 耦合分析 → ② 提升可见性(externalize)→ ③ 移动定义。
3. **Python 脚本可复现**:每阶段配套 `tools/_phaseNN*.py`,脚本内嵌目标符号清单与校验逻辑,运行后输出 before/after 行数。
4. **新 TU 头部**:沿用 `gbe_dota_handlers.cpp` 的 license + include 块模板,include 顺序保持一致以避免遗漏。
5. **内部共享头**:[dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h) 是各 split TU 之间共享符号的唯一入口,新增 extern 声明一律追加到 `#endif` 之前。
6. **premake 同步**:每新增一个 TU,必须同步更新 [premake5.lua](file:///workspace/premake5.lua) 中相应项目的文件列表(参照 `gbe_dota_handlers.cpp` 的加入方式)。

---

## 6. 参考文档

- 项目代码 Wiki:[CODE_WIKI.md](file:///workspace/CODE_WIKI.md) §4.8 GBE/Dota2 Game Coordinator 模块群
- Phase 2.3 耦合分析报告:[tools/_phase23_coupling_report.txt](file:///workspace/tools/_phase23_coupling_report.txt)
- 各阶段脚本:[tools/_phase23a_externalize.py](file:///workspace/tools/_phase23a_externalize.py)、[_phase23b_extract_handlers.py](file:///workspace/tools/_phase23b_extract_handlers.py)、[_phase24a_delete_dead.py](file:///workspace/tools/_phase24a_delete_dead.py)
