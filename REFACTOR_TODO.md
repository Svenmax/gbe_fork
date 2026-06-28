# steam_game_coordinator.cpp 重构进度与任务列表

> 本文件记录对 `dll/steam_game_coordinator.cpp`(原始 15029 行的"巨石"文件)进行分阶段拆分重构的完整进度与后续计划,便于会话切换后快速恢复上下文。
>
> **当前分支**: `trae/agent-inRF11`
> **最后提交**: `88e5cb4` — refactor(gc): extract lobby launch/teardown flow to gbe_dota_lobby_launch_coordinator.cpp (phase 2.6)

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

### Phase 2.4a — 删除死代码 + 为 Phase 2.4b 预共享符号(2026-06-28)

提交:`f0fbea7` refactor(gc): delete dead code and prep shared symbols for lobby-flow split (phase 2.4a)

- 脚本:[tools/_phase24a_delete_dead.py](file:///workspace/tools/_phase24a_delete_dead.py)
- 删除 18 个未引用的死代码 static 符号(5 函数 + 13 变量,共 ~443 行)
- 将 `GBE_DotaServerHelloContext` 结构体从 .cpp 移到 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h)
- 外部化 `GBE_BuildDirectDotaServerWelcome`(函数)与 `GBE_last_dota_server_hello_context`(变量)— 这两个是 "core GC code (stays)" 与 "lobby-flow code (will move)" 之间唯一需要共享的符号
- 主文件:`8862 → 8398` 行

### Phase 2.4b — 提取 lobby-flow helpers 到新 TU(2026-06-28,当前 HEAD)

提交:`4f5f4e3` refactor(gc): extract lobby-flow helpers to gbe_dota_lobby_flow_coordinator.cpp (phase 2.4b)

- 脚本:[tools/_phase24b_analyze.py](file:///workspace/tools/_phase24b_analyze.py)、[_phase24b_extract.py](file:///workspace/tools/_phase24b_extract.py)
- 耦合分析报告:[tools/_phase24b_coupling_report.txt](file:///workspace/tools/_phase24b_coupling_report.txt)
- 新增 [dll/gbe_dota_lobby_flow_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_flow_coordinator.cpp)(702 行)
- 移走 10 个 lobby-flow helper 成员函数(`GBE_ResetDotaPracticeLobbyLaunchPeripheralState` 等,L7018–L7632 区间)
- 移走 4 个 List X static 常量(`GBE_kDotaLaunchPersonaState*Hex`,仅被上述函数引用)
- List Y 为空 — Phase 2.4a + 早期阶段的预备工作已完整覆盖跨 TU 依赖,无需 externalize 任何新符号
- 主文件:`8398 → 7774` 行(-624 行)

### Phase 2.5 — 提取 lobby-snapshot/build helpers 到新 TU(2026-06-28,当前 HEAD)

提交:`a9be325` refactor(gc): extract lobby snapshot/build helpers to gbe_dota_lobby_snapshot_coordinator.cpp (phase 2.5)

- 脚本:[tools/_phase25_analyze.py](file:///workspace/tools/_phase25_analyze.py)、[_phase25_externalize.py](file:///workspace/tools/_phase25_externalize.py)、[_phase25_extract.py](file:///workspace/tools/_phase25_extract.py)
- 耦合分析报告:[tools/_phase25_coupling_report.txt](file:///workspace/tools/_phase25_coupling_report.txt)
- 新增 [dll/gbe_dota_lobby_snapshot_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_snapshot_coordinator.cpp)(683 行)
- 移走 10 个 lobby-snapshot/build helper 成员函数(8 个唯一名,2 个有 2 个重载,共 605 行函数体)
- **外部化 6 个 file-scope static 符号**(2 List Y + 4 List X→Y):
  - 2 List Y(目标函数与留主代码共享):`GBE_kDotaOfficial032PracticeLobby26Hex`(constexpr var)、`GBE_ReplayDotaPracticeLobbyOfficial26Payload`(function)
  - 4 List X→Y(原为 List X,但发现传递依赖 List Z static,故提升为 List Y,定义留在主文件):`GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate`、`GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload`、`GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl`、`GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl`
  - extern 声明追加到 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h)(188 → 374 行)
- **关键策略变更**:初始期望 4 个 List X static 函数可作为 `static` 跟随移走,但读取函数体后发现它们都调用 List Z static(留在主文件)。若移走会丢失对这些 static 符号的访问,故改为 externalize(定义留在主文件),仅移走 10 个成员函数定义。
- 主文件:`7774 → 7158` 行(-616 行)

### Phase 2.6 — 提取 lobby launch/teardown 流程到新 TU(2026-06-28,当前 HEAD)

提交:`88e5cb4` refactor(gc): extract lobby launch/teardown flow to gbe_dota_lobby_launch_coordinator.cpp (phase 2.6)

- 脚本:[tools/_phase26_analyze.py](file:///workspace/tools/_phase26_analyze.py)、[_phase26_extract.py](file:///workspace/tools/_phase26_extract.py)
- 耦合分析报告:[tools/_phase26_coupling_report.txt](file:///workspace/tools/_phase26_coupling_report.txt)
- 新增 [dll/gbe_dota_lobby_launch_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_launch_coordinator.cpp)(722 行)
- 移走 13 个 lobby launch/teardown 流程成员函数(619 行):`GBE_TryQueueDotaPrelaunch021` / `GBE_SetDotaLobbyMemberConnected/RuntimeState` / `GBE_ShouldHoldDotaLanLaunchForRemoteMembers` / `GBE_TryQueueDotaRuntimeLobbyDetailsUpdate` / `GBE_HasDotaLaunchServerSetupSync` / `GBE_MarkDotaLaunchPhase` / `GBE_TryAdvanceDotaLaunchToRun` / `GBE_PushDotaLaunchStateToClientPeer` / `GBE_QueueDotaPostGameTeardown` / `GBE_SendDotaPracticeLobbyDetailsUpdate` / `GBE_PushDotaResponse` / `GBE_SendDotaCustomGameLaunchSetupFlow`
- 移走 2 个 List X static 符号(保持 `static`,作为新 TU 内部符号):`GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex`(constexpr var)、`GBE_GenerateDotaPostGameChatChannelId`(function)。两者均无传递依赖 List Z(一个是纯数据常量,另一个只用 std::random/chrono)
- **List Y = 0** — 无需 externalize,早期阶段(2.2/2.3a/2.4a/2.5)已覆盖所有跨 TU 共享需求,header 无改动
- 主文件:`7158 → 6510` 行(-648 行)

---

## 3. 当前文件布局

> 以下反映 Phase 2.6 提交后的状态。

| 文件 | 行数 | 职责 |
|---|---:|---|
| [dll/steam_game_coordinator.cpp](file:///workspace/dll/steam_game_coordinator.cpp) | 6510 | GC 类核心:基础设施(SendMessage_/RetrieveMessage/network_callback/RunCallbacks)、lobby-state 状态机、welcome/cache/payload 构造、路由分发、`handle_dota_client_message` |
| [dll/gbe_dota_handlers.cpp](file:///workspace/dll/gbe_dota_handlers.cpp) | 6335 | 62 个 `GBE_HandleDota*` 请求 handler + 23 个 handler-only static 符号 |
| [dll/gbe_dota_lobby_flow_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_flow_coordinator.cpp) | 702 | 10 个 lobby-flow helper 成员函数 + 4 个 List X static 常量(Phase 2.4b 新增) |
| [dll/gbe_dota_lobby_snapshot_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_snapshot_coordinator.cpp) | 683 | 10 个 lobby-snapshot/build helper 成员函数(Phase 2.5 新增) |
| [dll/gbe_dota_lobby_launch_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_launch_coordinator.cpp) | 722 | 13 个 lobby launch/teardown 流程成员函数 + 2 个 List X static 符号(Phase 2.6 新增) |
| [dll/gbe_dota_lobby_state_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_state_coordinator.cpp) | 1701 | lobby-state 成员管理 |
| [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h) | 374 | 跨 TU 共享的内部头:外部链接符号声明、`GBE_DotaServerHelloContext` 结构、`ser_var` 模板 |
| **合计** | **17027** | |

> Phase 2.6 变更:主文件 -648 行,新 TU +722 行(含 78 行 license/include 头),header 无改动。原始主文件 15029 行 → 当前 6510 行,累计缩减 57%。

主文件 `steam_game_coordinator.cpp` 当前剩余的 `Steam_Game_Coordinator::` 成员函数布局(行号 → 函数):

```
L3200  GBE_DispatchDotaPostLoginRequest
L3275  gc_enabled / client_items / server_items / parse_gc_config / is_welcome_message
L3336  GBE_ApplyQueuedLobbyState
L3407  push_incoming
L3436  GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon
L3452  GBE_DiscardQueuedDotaLaunchMessagesForAbandon
L3505  GBE_ShouldSuppressDotaAbandonedLobby
L3510  GBE_MarkDotaAbandonedLobbySuppressed
L3541  GBE_ClearDotaAbandonedLobbySuppression
L3557  push_incoming_now
L3589  build_msg_header / parse_msg_header / build_protomsg_header / parse_protomsg
L3645  GBE_PatchDotaLoginCacheSubscribedInventory
L3928  item_id_local_to_network / item_id_network_to_local
L3962  item_to_gcstruct / item_to_gcprotobuf
L4062  handle_set_item_pos / handle_delete_item / handle_motd_request
       handle_respawn / handle_set_item_style / handle_adjust_equip_state
       handle_set_multiple_item_pos
L4239  callback_client_welcome / callback_server_welcome
L4270  GBE_MaybePrimeDotaServerWelcomeFromCache
L4377  callback_items_received / callback_items_removed
       callback_item_updated / callback_item_deleted / callback_respawn_request
L4625  steam_network_callback / steam_run_every_runcb
L4692  initialize_gc / shutdown_gc / on_appid_changed
L4802  load_items_from_file / save_items_to_file
L4990  set_item_pos / delete_item / request_user_items / find_items_request
       remove_user_items / on_client_connected / on_client_disconnected
L5208  GBE_PushDotaLoginSyncMessages
L5423  GBE_GetDotaJoinableCustomLobbiesHTTPJSON
L5474  ResetGCMemory
L5530  GBE_GetDotaLobbyOwnerName
L5544  handle_dota_client_message            ← 核心分发,留在主文件
L5747  SendMessage_                          ← 核心基础设施,留在主文件
L5817  IsMessageAvailable
L5854  RetrieveMessage
L5984  network_callback_inventory_request
L6047  network_callback_inventory_response
L6171  network_callback_item_update
L6227  network_callback_item_deletion
L6268  network_callback_respawn_request
L6281  network_callback
L6354  RunCallbacks
```

> 注:Phase 2.6 移走了原 L5430–L6190 区间交错的 13 个 lobby launch/teardown 流程函数(619 行)+ L267/L1599 的 2 个 List X static 符号(13 行),共 -648 行(含尾随空行)。故上表 L5423+ 段相对 Phase 2.5 前移。主文件现仅剩核心基础设施、inventory/item 相关、welcome/hello 相关与 `handle_dota_client_message` 分发,后续可按 Phase 2.7/2.8/2.9 继续拆分。

---

## 4. 待完成任务

### Phase 2.5 / 2.6 — 已完成 ✅

详见 §2 完成记录。提交:`a9be325`(Phase 2.5)、`88e5cb4`(Phase 2.6)。

### Phase 2.7+ — 后续路线图(待规划,优先级递减)

主文件 6510 行仍然偏大,后续可继续按职责拆分。候选方向(需各自做耦合分析后再定):

- **2.7**:提取 inventory/item 相关函数(`item_id_*` / `item_to_*` / `handle_set_item_*` / `handle_delete_item` / `callback_items_*` / `load_items_from_file` / `save_items_to_file` / `set_item_pos` / `delete_item` / `find_items_request` / `remove_user_items`)— L3928–L5208 区间
- **2.8**:提取 welcome/hello 相关函数(`GBE_PatchDotaLoginCacheSubscribedInventory` / `callback_client_welcome` / `callback_server_welcome` / `GBE_MaybePrimeDotaServerWelcomeFromCache` / `GBE_PushDotaLoginSyncMessages`)— L3645–L5208 区间
- **2.9**:把主文件中剩余的纯函数 static helpers(`GBE_PatchDota*` / `GBE_ReplayDota*` / `GBE_AdaptDota*` / `GBE_ExtractDota*` / `GBE_PrepareDota*` / `GBE_BuildDirectDota*Welcome` / `GBE_ComposeDota*Welcome`)按主题归并到对应 TU,或单独建 `gbe_dota_gc_payload_helpers.cpp`

完成 2.7–2.9 后,主文件预期可缩减至 ~2000 行,仅保留 GC 类的核心基础设施(初始化、消息收发骨架、回调路由、网络回调入口)与 `handle_dota_client_message` 顶层分发。

---

## 5. 关键设计约定(沿用既有约定)

1. **行为零变化**:所有阶段仅做物理重组,不改逻辑、不改签名、不改 ABI。
2. **三步法**:每个拆分阶段遵循 ① 耦合分析 → ② 提升可见性(externalize)→ ③ 移动定义。
3. **Python 脚本可复现**:每阶段配套 `tools/_phaseNN*.py`,脚本内嵌目标符号清单与校验逻辑,运行后输出 before/after 行数。
4. **新 TU 头部**:沿用 `gbe_dota_handlers.cpp` 的 license + include 块模板,include 顺序保持一致以避免遗漏。
5. **内部共享头**:[dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h) 是各 split TU 之间共享符号的唯一入口,新增 extern 声明一律追加到 `#endif` 之前。
6. **premake 同步**:[premake5.lua](file:///workspace/premake5.lua) 的 `common_files` 用 `"dll/**"` 通配符自动收集 `dll/` 下所有源文件,新增 TU 无需手动修改构建脚本。

---

## 6. 参考文档

- 项目代码 Wiki:[CODE_WIKI.md](file:///workspace/CODE_WIKI.md) §4.8 GBE/Dota2 Game Coordinator 模块群
- Phase 2.3 耦合分析报告:[tools/_phase23_coupling_report.txt](file:///workspace/tools/_phase23_coupling_report.txt)
- Phase 2.4b 耦合分析报告:[tools/_phase24b_coupling_report.txt](file:///workspace/tools/_phase24b_coupling_report.txt)
- Phase 2.5 耦合分析报告:[tools/_phase25_coupling_report.txt](file:///workspace/tools/_phase25_coupling_report.txt)
- Phase 2.6 耦合分析报告:[tools/_phase26_coupling_report.txt](file:///workspace/tools/_phase26_coupling_report.txt)
- 各阶段脚本:[tools/_phase23a_externalize.py](file:///workspace/tools/_phase23a_externalize.py)、[_phase23b_extract_handlers.py](file:///workspace/tools/_phase23b_extract_handlers.py)、[_phase24a_delete_dead.py](file:///workspace/tools/_phase24a_delete_dead.py)、[_phase24b_extract.py](file:///workspace/tools/_phase24b_extract.py)、[_phase25_externalize.py](file:///workspace/tools/_phase25_externalize.py)、[_phase25_extract.py](file:///workspace/tools/_phase25_extract.py)、[_phase25_fix_default_args.py](file:///workspace/tools/_phase25_fix_default_args.py)、[_phase26_analyze.py](file:///workspace/tools/_phase26_analyze.py)、[_phase26_extract.py](file:///workspace/tools/_phase26_extract.py)
