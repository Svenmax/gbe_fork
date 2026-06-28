# steam_game_coordinator.cpp 重构进度与任务列表

> 本文件记录对 `dll/steam_game_coordinator.cpp`(原始 15029 行的"巨石"文件)进行分阶段拆分重构的完整进度与后续计划,便于会话切换后快速恢复上下文。
>
> **当前分支**: `trae/agent-inRF11`
> **最后提交**: `a4e7eaf` — refactor(gc): extract network_callback_* to gbe_dota_network_callbacks.cpp (phase 2.10)

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

### Phase 2.7 — 提取 inventory/item 函数到新 TU(2026-06-28,当前 HEAD)

提交:`be7d394` refactor(gc): extract inventory/item functions to gbe_dota_inventory_coordinator.cpp (phase 2.7)
后续修复:`cf3b94f`(GCMsgHdrEx_t 结构体可见性)、`fe08e73`(parse_protomsg 显式实例化)

- 脚本:[tools/_phase27_analyze.py](file:///workspace/tools/_phase27_analyze.py)、[_phase27_externalize.py](file:///workspace/tools/_phase27_externalize.py)、[_phase27_extract.py](file:///workspace/tools/_phase27_extract.py)
- 耦合分析报告:[tools/_phase27_coupling_report.txt](file:///workspace/tools/_phase27_coupling_report.txt)
- 新增 [dll/gbe_dota_inventory_coordinator.cpp](file:///workspace/dll/gbe_dota_inventory_coordinator.cpp)(909 行)
- 移走 20 个 inventory/item 成员函数(797 行):`item_id_local_to_network` / `item_id_network_to_local` / `item_to_gcstruct` / `item_to_gcprotobuf` / `handle_set_item_pos` / `handle_delete_item` / `handle_set_item_style` / `handle_adjust_equip_state` / `handle_set_multiple_item_pos` / `callback_items_received` / `callback_items_removed` / `callback_item_updated` / `callback_item_deleted` / `load_items_from_file` / `save_items_to_file` / `set_item_pos` / `delete_item` / `request_user_items` / `find_items_request` / `remove_user_items`
- 移走 1 个 List X static 符号(保持 `static`,作为新 TU 内部符号):`ser_varstring`(function)。验证无传递依赖 List Z(仅调用 `ser_var<uint16>` 内联模板 + `std::string::append`)
- **外部化 1 个 List Y 符号**:`deser_var` 模板(template function)。因模板不能像普通函数那样 `extern`,将定义从 .cpp 移到 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h)(以 `template <class T> inline` 形式,与 Phase 2.3a 的 `ser_var` 同模式),并在 header 补 `#include <cstring>`(memcpy)
- 主文件:`6510 → 5666` 行(-844 行:externalize -9 + extract -828 + struct fix -14 + explicit instantiation +7)
- 累计缩减:原始 15029 行 → 当前 5666 行,**缩减 62.3%**
- **构建修复(2 轮)**:
  - 第 1 轮 `be7d394` 构建 #967 失败 — C2079/C2027:新 TU 使用 `GCMsgHdrEx_t`(by value)但仅有 forward declaration,完整定义在主文件 .cpp 内。修复 `cf3b94f`:将 `GCMsgHdrEx_t` 结构体定义(含 `#pragma pack`)移到 [dll/dll/steam_game_coordinator.h](file:///workspace/dll/dll/steam_game_coordinator.h) 替换 forward declaration(`JobID_t` 已通过 `base.h` 可用);主文件仅保留 `GCMsgHdr_t`。脚本:[_phase27_fix_struct_visibility.py](file:///workspace/tools/_phase27_fix_struct_visibility.py)
  - 第 2 轮 `cf3b94f` 构建 #968 失败 — LNK2019:新 TU 调用 `parse_protomsg<T>`(member function template),但模板定义在主文件 .cpp,其他 TU 无法实例化。修复 `fe08e73`:在主文件模板定义后追加 2 行 explicit instantiation(`CMsgAdjustItemEquippedState` / `CMsgSetItemPositions`),强制编译器生成符号供链接器解析

### Phase 2.8 — 提取 welcome/hello 函数到新 TU(2026-06-28,当前 HEAD)

提交:`adc8e04` refactor(gc): extract welcome/hello functions to gbe_dota_welcome_coordinator.cpp (phase 2.8)

- 脚本:[tools/_phase28_analyze.py](file:///workspace/tools/_phase28_analyze.py)、[_phase28_extract.py](file:///workspace/tools/_phase28_extract.py)
- 耦合分析报告:[tools/_phase28_coupling_report.txt](file:///workspace/tools/_phase28_coupling_report.txt)
- 新增 [dll/gbe_dota_welcome_coordinator.cpp](file:///workspace/dll/gbe_dota_welcome_coordinator.cpp)(979 行)
- 移走 5 个 welcome/hello/login-sync 成员函数(452 行):`GBE_PatchDotaLoginCacheSubscribedInventory` / `callback_client_welcome` / `callback_server_welcome` / `GBE_MaybePrimeDotaServerWelcomeFromCache` / `GBE_PushDotaLoginSyncMessages`
- 移走 1 个 List X static 符号(保持 `static`,作为新 TU 内部符号):`GBE_kDotaCacheSubscribedTemplate`(`static const uint8[]` 字节模板,470 行)。验证无传递依赖 List Z(纯数据数组,无函数调用)
- **List Y = 0** — 无需 externalize,早期阶段(2.2/2.3a/2.4a/2.5)已覆盖所有跨 TU 共享需求,header 无改动
- **分析脚本修复**:初版 `collect_static_symbols` 的正则 `[\(\{=;]` 无法匹配数组定义(`NAME[]`),漏掉 3 个 `static const uint8[]` 数组。修复为 `[\(\{=;\[]` 后正确识别 1 个 List X + 2 个 List Z
- 主文件:`5666 → 4738` 行(-928 行)
- 累计缩减:原始 15029 行 → 当前 4738 行,**缩减 68.5%**
- **构建一次通过**(commit `adc8e04`,无需修复)

### Phase 2.9 — 提取剩余 file-scope static payload helpers 到新 TU(2026-06-28,当前 HEAD)

提交:`65a0466` refactor(gc): extract payload helpers to gbe_dota_gc_payload_helpers.cpp (phase 2.9)
后续修复:`4d1d5c5`(默认参数重复 / extern 链接 / extern 数组 sizeof)

- 脚本:[tools/_phase29_analyze.py](file:///workspace/tools/_phase29_analyze.py)、[_phase29_extract.py](file:///workspace/tools/_phase29_extract.py)
- 耦合分析报告:[tools/_phase29_coupling_report.txt](file:///workspace/tools/_phase29_coupling_report.txt)
- 新增 [dll/gbe_dota_gc_payload_helpers.cpp](file:///workspace/dll/gbe_dota_gc_payload_helpers.cpp)(1492 行)
- 将主文件中**全部 28 个 file-scope static 定义**(27 个唯一名,含函数与 const 数据常量)整体迁移到新 TU。这是首次针对"非成员、纯 file-scope static helper"的归并阶段,主文件自此**不再含任何 file-scope static 定义**(`grep "^static " dll/steam_game_coordinator.cpp` 为空)
- 符号分类(共 28 个定义):
  - **List X = 9**(仅被其他移走的 helper 调用):保持 `static`,作为新 TU 内部符号。包含 `GBE_kDotaClientWelcomeTemplate`、`GBE_kOldDotaVersionVarint`、`GBE_kOldDotaPracticeLobbyLobbyIdVarint`、`GBE_kOldDotaPracticeLobbyGameStartTimeVarint`、`GBE_kOldDotaPracticeLobbyConnect`、`GBE_PatchDotaWelcomeAccountObjects`、`GBE_PatchDotaWelcomeAccountObjectsField2`、`GBE_PatchDotaWelcomeAccountObjectsField3`、`GBE_PatchDotaWelcomeAccountObjectsField5`
  - **List Y = 18**(被主文件留存的成员函数或已 externalized 函数调用):去 `static`,在 header 加 extern 声明。含 2 个 const 数据变量(`GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex`、`GBE_kDotaPracticeLobbyCacheSubscribedTemplate[312]`)与 16 个函数(`GBE_LogGCProtoBoundary`、`GBE_ForceDotaLobbyUpdateOwnerSOID`、`GBE_PatchDotaPracticeLobbyLaunchTemplate`、`GBE_PatchDotaLobbyTemplateIdentifiers`、`GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState`、`GBE_ComposeDotaPracticeLobbySOObjects`、`GBE_AdaptDotaTopCustomGamesListPayload`、`GBE_IsDotaOtherLeftChannelPayloadForChannel`、`GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload`、`GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload`、`GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate`、`GBE_ExtractDotaHelloContext`、`GBE_ExtractDirectDotaHelloContext`、`GBE_ExtractDirectDotaServerHelloContext`、`GBE_BuildDirectDotaClientWelcome`、`GBE_ComposeDotaClientWelcome`)
  - **List Z = 0**(无被其他 TU 引用的 static)
  - **Unused = 0**
- **结构体可见性调整**:将 `struct GBE_DotaHelloContext` 定义从主 .cpp 移到 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h)(新 TU 的 List Y 函数 `GBE_ExtractDotaHelloContext` 等需要完整类型,前向声明不够)。同 Phase 2.4a 对 `GBE_DotaServerHelloContext` 的处理模式
- **附带死代码迁移**:`struct GBE_DotaGenericLobbyEntry`(原主文件中的死代码,全代码库无引用)被 extract 脚本一并带到新 TU,不影响构建
- **耦合分析误分类修正**:初版 analyze 脚本将 3 个符号误判为 List X,实际被 Phase 2.5 externalized 的函数(留在主文件)调用,应为 List Y:`GBE_PatchDotaLobbyTemplateIdentifiers`、`GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState`、`GBE_ComposeDotaPracticeLobbySOObjects`。手动在新 TU 移除 `static` 前缀并在 header 补 extern 声明(含 `gbe::gc_message::DotaPracticeLobbyObjects` 前向声明)
- 主文件:`4738 → 3270` 行(-1468 行)
- header:`384 → 629` 行(+245 行:18 个 extern 声明 + `GBE_DotaHelloContext` 结构体 + `DotaPracticeLobbyObjects` 前向声明)
- 累计缩减:原始 15029 行 → 当前 3270 行,**缩减 78.2%**
- **构建修复(1 轮)**:
  - 第 1 轮 `65a0466` 构建失败,5 个编译错误:
    - **4× C2572**(redefinition of default argument):4 个 List Y 函数定义中重复了默认参数 `custom_game = nullptr`,而 header 声明已有默认参数。C++ 规则:默认参数只能在声明或定义之一出现。修复:在新 TU 的 4 处定义中移除 `= nullptr`(`GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState`、`GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload`、`GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload`、`GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate`)
    - **1× C2070**(ill-formed sizeof operand):主文件 `sizeof(GBE_kDotaPracticeLobbyCacheSubscribedTemplate)` 对 header 中 `extern const uint8 NAME[]`(无大小)的数组无法计算。修复:在 header 声明中提供大小 `[312]`(由脚本统计数组元素数)
    - **latent linker risk**(`const` 内部链接):`const` 变量在命名空间作用域默认有内部链接,新 TU 的定义不会被主 TU 链接到。修复:在新 TU 的 2 个 List Y const 变量定义前加 `extern` 前缀
  - 修复提交 `4d1d5c5` 推送后构建成功(`win / build (api_experimental, x64, release)` = success)

### Phase 2.10 — 提取 network_callback_* 成员函数到新 TU(2026-06-28,当前 HEAD)

提交:`a4e7eaf` refactor(gc): extract network_callback_* to gbe_dota_network_callbacks.cpp (phase 2.10)

- 脚本:[tools/_phase30_analyze.py](file:///workspace/tools/_phase30_analyze.py)、[_phase30_extract.py](file:///workspace/tools/_phase30_extract.py)
- 耦合分析报告:[tools/_phase30_coupling_report.txt](file:///workspace/tools/_phase30_coupling_report.txt)
- 新增 [dll/gbe_dota_network_callbacks.cpp](file:///workspace/dll/gbe_dota_network_callbacks.cpp)(426 行)
- 移走 6 个 `Steam_Game_Coordinator::` network_callback 成员函数(共 359 行函数体):
  - `network_callback_inventory_request`(61 行)— 服务器请求本地 inventory,构造 InventoryResponse 回送
  - `network_callback_inventory_response`(122 行)— 接收远端玩家 inventory,落库 + 触发 callback_items_received;Dota2 LAN 服务端场景下额外推送 CacheSubscribed
  - `network_callback_item_update`(54 行)— 远端玩家更新单个 item(inv_pos/style/equip_states)
  - `network_callback_item_deletion`(39 行)— 远端玩家删除 item
  - `network_callback_respawn_request`(11 行)— 玩家换装后请求 respawn
  - `network_callback`(72 行)— 顶层网络消息分发器(gameserver_items_messages / friend_messages / steam_messages / low_level)
- **List X = 0**(主文件经 Phase 2.9 后已无 file-scope static,无 static 可随移)
- **List Y = 0**(无需 externalize):耦合分析逐符号验证,6 个函数引用的全部符号均已可见:
  - 成员函数/成员变量:经 [dll/dll/steam_game_coordinator.h](file:///workspace/dll/dll/steam_game_coordinator.h) class 定义可见(`callback_items_received`/`callback_item_updated`/`callback_item_deleted`/`callback_respawn_request`/`request_user_items`/`remove_user_items`/`server_items()`/`items`/`all_user_items`/`pending_items_requests`/`gc_profile`/`gc_initialized`/`is_server`/`settings`/`network`/`GBE_local_lobby`/`GBE_HandleDota*`/`GBE_MaybeHandleDota*`/`GBE_GetDotaLobbyOwnerSteamId`/`GBE_GC_DebugLog`)
  - free function `GBE_PushDotaPlayerEquippedItemsCacheToGC`:已在 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h#L93) L93 声明(Phase 2.9)
  - free function `GBE_GC_DebugLog`:已在 [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h#L17) L17 声明
  - `get_steam_client`:在 [dll/dll/dll.h](file:///workspace/dll/dll/dll.h#L31) L31 声明,且 dll.h L21 include `steam_client.h` 使 `Steam_Client` 完整定义可见,故 `steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota()` 可解析(`RefreshLobbyCallbacksForDota` 在 [dll/dll/steam_matchmaking.h](file:///workspace/dll/dll/steam_matchmaking.h#L137) L137 声明;`GC_PROFILE_DOTA2` 在 class enum L84)
  - `generate_steam_api_call_id`:在 [dll/dll/base.h](file:///workspace/dll/dll/base.h#L44) L44 声明
  - `check_econ_item_name`/`check_econ_item_desc`:在 [dll/dll/econ_item.h](file:///workspace/dll/dll/econ_item.h#L86) L86/L94 内联定义(Phase 2.7 inventory_coordinator 已验证可见)
- **include 块**:沿用 `gbe_dota_inventory_coordinator.cpp` 的 license + include 模板(已证明可解析相同依赖面);`gbe_dota_handlers.cpp`/`gbe_dota_lobby_state_coordinator.cpp` 等已有 TU 用相同 include 块访问 `steam_matchmaking->` 编译通过,佐证 include 链完整
- 主文件:`3270 → 2900` 行(-370 行,含 6 个函数体 + 前导注释 + 间隔空行)
- 累计缩减:原始 15029 行 → 当前 2900 行,**缩减 80.7%**
- **构建一次通过**(commit `a4e7eaf`,无需修复)

---

## 3. 当前文件布局

> 以下反映 Phase 2.10 提交后的状态。

| 文件 | 行数 | 职责 |
|---|---:|---|
| [dll/steam_game_coordinator.cpp](file:///workspace/dll/steam_game_coordinator.cpp) | 2900 | GC 类核心:基础设施(SendMessage_/RetrieveMessage/RunCallbacks)、lobby-state 状态机、路由分发、`handle_dota_client_message`、连接生命周期(`on_client_connected/disconnected`)。不再含 file-scope static,亦不再含 network_callback_* 系列成员 |
| [dll/gbe_dota_handlers.cpp](file:///workspace/dll/gbe_dota_handlers.cpp) | 6335 | 62 个 `GBE_HandleDota*` 请求 handler + 23 个 handler-only static 符号 |
| [dll/gbe_dota_gc_payload_helpers.cpp](file:///workspace/dll/gbe_dota_gc_payload_helpers.cpp) | 1492 | 28 个 file-scope static payload helpers(9 List X + 18 List Y)+ 附带死代码 `GBE_DotaGenericLobbyEntry`(Phase 2.9 新增) |
| [dll/gbe_dota_welcome_coordinator.cpp](file:///workspace/dll/gbe_dota_welcome_coordinator.cpp) | 979 | 5 个 welcome/hello/login-sync 成员函数 + 1 个 List X static(`GBE_kDotaCacheSubscribedTemplate`)(Phase 2.8 新增) |
| [dll/gbe_dota_inventory_coordinator.cpp](file:///workspace/dll/gbe_dota_inventory_coordinator.cpp) | 909 | 20 个 inventory/item 成员函数 + 1 个 List X static(`ser_varstring`)(Phase 2.7 新增) |
| [dll/gbe_dota_network_callbacks.cpp](file:///workspace/dll/gbe_dota_network_callbacks.cpp) | 426 | 6 个 network_callback_* 成员函数(inventory request/response、item update/deletion、respawn request、顶层 dispatcher)(Phase 2.10 新增) |
| [dll/gbe_dota_lobby_launch_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_launch_coordinator.cpp) | 722 | 13 个 lobby launch/teardown 流程成员函数 + 2 个 List X static 符号(Phase 2.6 新增) |
| [dll/gbe_dota_lobby_flow_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_flow_coordinator.cpp) | 702 | 10 个 lobby-flow helper 成员函数 + 4 个 List X static 常量(Phase 2.4b 新增) |
| [dll/gbe_dota_lobby_snapshot_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_snapshot_coordinator.cpp) | 683 | 10 个 lobby-snapshot/build helper 成员函数(Phase 2.5 新增) |
| [dll/gbe_dota_lobby_state_coordinator.cpp](file:///workspace/dll/gbe_dota_lobby_state_coordinator.cpp) | 1701 | lobby-state 成员管理 |
| [dll/gbe_dota_gc_internal.h](file:///workspace/dll/gbe_dota_gc_internal.h) | 629 | 跨 TU 共享的内部头:外部链接符号声明、`GBE_DotaServerHelloContext`/`GBE_DotaHelloContext` 结构、`ser_var`/`deser_var` 模板 |
| **合计** | **17156** | |

> Phase 2.10 变更:主文件 -370 行(6 个 network_callback_* 成员函数迁出),新 TU +426 行(含 84 行 license/include 头),header 无改动。原始主文件 15029 行 → 当前 2900 行,累计缩减 80.7%。

主文件 `steam_game_coordinator.cpp` 当前剩余的 `Steam_Game_Coordinator::` 成员函数布局(行号 → 函数):

```
L1227  GBE_DispatchDotaPostLoginRequest
L1302  gc_enabled / client_items / server_items / parse_gc_config / is_welcome_message
L1363  GBE_ApplyQueuedLobbyState
L1434  push_incoming
L1463  GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon
L1479  GBE_DiscardQueuedDotaLaunchMessagesForAbandon
L1532  GBE_ShouldSuppressDotaAbandonedLobby
L1537  GBE_MarkDotaAbandonedLobbySuppressed
L1568  GBE_ClearDotaAbandonedLobbySuppression
L1584  push_incoming_now
L1616  build_msg_header / parse_msg_header / build_protomsg_header / parse_protomsg (template)
       (+ L1676/L1677 显式实例化: CMsgAdjustItemEquippedState / CMsgSetItemPositions)
L1679  handle_motd_request / handle_respawn
L1713  callback_respawn_request
L1725  steam_network_callback / steam_run_every_runcb
L1792  initialize_gc / shutdown_gc / on_appid_changed
L1902  on_client_connected / on_client_disconnected
L2183  GBE_GetDotaJoinableCustomLobbiesHTTPJSON
L2234  ResetGCMemory
L2290  GBE_GetDotaLobbyOwnerName
L2304  handle_dota_client_message            ← 核心分发,留在主文件
L2507  SendMessage_                          ← 核心基础设施,留在主文件
L2577  IsMessageAvailable
L2614  RetrieveMessage
L2744  RunCallbacks
```

> 注:Phase 2.10 移走了 6 个 network_callback_* 成员函数(原 L2744–L3112 区间,共 -370 行)。主文件现仅剩核心基础设施(SendMessage_/RetrieveMessage/RunCallbacks 等)、lobby-state 状态机、连接生命周期(`on_client_connected/disconnected`)、`handle_dota_client_message` 顶层分发,以及跨 TU 共享状态变量。network_callback_* 系列已迁至 [dll/gbe_dota_network_callbacks.cpp](file:///workspace/dll/gbe_dota_network_callbacks.cpp)。主文件已无任何 file-scope static 定义(Phase 2.9 完成)。

---

## 4. 待完成任务

### Phase 2.5 / 2.6 / 2.7 / 2.8 / 2.9 / 2.10 — 已完成 ✅

详见 §2 完成记录。提交:`a9be325`(Phase 2.5)、`88e5cb4`(Phase 2.6)、`be7d394`+`cf3b94f`+`fe08e73`(Phase 2.7)、`adc8e04`(Phase 2.8)、`65a0466`+`4d1d5c5`(Phase 2.9)、`a4e7eaf`(Phase 2.10)。

### Phase 2.11+ — 后续路线图(待规划,优先级递减)

Phase 2.10 已将 network_callback_* 系列迁出,主文件降至 2900 行(累计缩减 80.7%)。剩余 2900 行主要是 `Steam_Game_Coordinator::` 类核心成员函数。后续可继续按职责拆分。候选方向(需各自做耦合分析后再定):

- **2.11**:把 `on_client_connected` / `on_client_disconnected`(L1902–L2182,~280 行)的连接生命周期处理逻辑拆到新 TU(如 `gbe_dota_connection_lifecycle.cpp`)
- **2.12**:评估 `handle_dota_client_message`(L2304–L2506,~200 行顶层分发)是否值得单独成 TU,或保留在主文件作为分发中枢
- **2.13**:评估 `SendMessage_` / `RetrieveMessage` / `IsMessageAvailable`(L2507–L2743,~237 行消息收发基础设施)是否值得单独成 TU

完成上述阶段后,主文件预期可缩减至 ~2000 行,仅保留 GC 类的核心骨架(初始化、回调注册入口)与 `handle_dota_client_message` 顶层分发。

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
- Phase 2.7 耦合分析报告:[tools/_phase27_coupling_report.txt](file:///workspace/tools/_phase27_coupling_report.txt)
- Phase 2.8 耦合分析报告:[tools/_phase28_coupling_report.txt](file:///workspace/tools/_phase28_coupling_report.txt)
- Phase 2.9 耦合分析报告:[tools/_phase29_coupling_report.txt](file:///workspace/tools/_phase29_coupling_report.txt)
- Phase 2.10 耦合分析报告:[tools/_phase30_coupling_report.txt](file:///workspace/tools/_phase30_coupling_report.txt)
- 各阶段脚本:[tools/_phase23a_externalize.py](file:///workspace/tools/_phase23a_externalize.py)、[_phase23b_extract_handlers.py](file:///workspace/tools/_phase23b_extract_handlers.py)、[_phase24a_delete_dead.py](file:///workspace/tools/_phase24a_delete_dead.py)、[_phase24b_extract.py](file:///workspace/tools/_phase24b_extract.py)、[_phase25_externalize.py](file:///workspace/tools/_phase25_externalize.py)、[_phase25_extract.py](file:///workspace/tools/_phase25_extract.py)、[_phase25_fix_default_args.py](file:///workspace/tools/_phase25_fix_default_args.py)、[_phase26_analyze.py](file:///workspace/tools/_phase26_analyze.py)、[_phase26_extract.py](file:///workspace/tools/_phase26_extract.py)、[_phase27_analyze.py](file:///workspace/tools/_phase27_analyze.py)、[_phase27_externalize.py](file:///workspace/tools/_phase27_externalize.py)、[_phase27_extract.py](file:///workspace/tools/_phase27_extract.py)、[_phase27_fix_struct_visibility.py](file:///workspace/tools/_phase27_fix_struct_visibility.py)、[_phase28_analyze.py](file:///workspace/tools/_phase28_analyze.py)、[_phase28_extract.py](file:///workspace/tools/_phase28_extract.py)、[_phase29_analyze.py](file:///workspace/tools/_phase29_analyze.py)、[_phase29_extract.py](file:///workspace/tools/_phase29_extract.py)、[_phase30_analyze.py](file:///workspace/tools/_phase30_analyze.py)、[_phase30_extract.py](file:///workspace/tools/_phase30_extract.py)
