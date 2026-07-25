# 消息路由清单（四轨真相表）

> 源码扫描日期：2026-07-25（direct conditional fallback、wrapped hard miss 与 registry-defensive template routing boundary 显式化）。改入口必须同步更新本表。
> 分发顺序（post-login）：**registry → 条件/观察 fallback → template_replay**。
> Hello / ServerHello 经 `handle_dota_client_message` 转调 welcome handlers（不进 post-login registry）。

## 0. 分发骨架

| 阶段 | 位置 | 说明 |
|------|------|------|
| 入口 | `Steam_Game_Coordinator::SendMessage_` → `handle_dota_client_message` | `dll/steam_game_coordinator.cpp` |
| ServerHello | `GBE_HandleDotaServerHelloRequest` | masked `GBE_kEMsgGCServerHello` (4007) → welcome_coordinator |
| ClientHello | `GBE_HandleDotaClientHelloRequest(direct=true)` | masked `GBE_kEMsgGCClientHello` (4006) |
| ClientToGC Hello | `GBE_HandleDotaClientHelloRequest(direct=false)` | 先 `GBE_ExtractDotaHelloContext` |
| ClientToGC 非 Hello | `GBE_HandleDotaWrappedPostLoginRequest` | registry → dead 7047 fallback |
| 其它 direct | `GBE_HandleDotaDirectPostLoginRequest` | registry → 条件路径 → template |
| Registry 表 | `GBE_ProductionDotaHandlerRegistry` | `dll/gbe_dota_post_login_dispatcher.cpp`（**52** 条） |
| Template | `GBE_HandleDotaTemplateReplayRequest` | `dll/gbe_dota_template_replay_handlers.cpp` |

**硬规则：** 新增 emsg 只加 registry 行；禁止新增 if-chain / 默认 template case（template-only 白名单除外）。

---

## 1. Registry（生产 kTable，52）

| emsg | 常量/名 | HandlerId | modes | lifecycle | fixture / 测 |
|------|---------|-----------|-------|-----------|---------------|
| 7009 | JoinChatChannel | JoinChatChannel | D+W | LobbyMutation | smoke:test_chat_join_channel |
| 7038 | PracticeLobbyCreate | PracticeLobbyCreate | D+W | LobbyLifecycle | replay:lobby_lifecycle:lobby_create_with_passkey |
| 8011 | LobbyList | LobbyList | D+W | LobbyRead | — |
| 7042 | CustomLobbyListRequest | CustomLobbyList | D+W | LobbyRead | — |
| 7111 | FriendPracticeLobbyList | FriendPracticeLobbyList | D+W | LobbyRead | — |
| 4512 | GCInviteToLobby | InviteToLobby | D+W | LobbyMutation | smoke:…invite… |
| 4513 | GCLobbyInviteResponse | LobbyInviteResponse | D+W | LobbyMutation | smoke:…decline… |
| 7044 | PracticeLobbyJoin | PracticeLobbyJoin | D+W | LobbyLifecycle | replay:lobby_lifecycle:lobby_join_by_id |
| 7040 | PracticeLobbyLeave | PracticeLobbyLeave | D+W | LobbyLifecycle | replay:game_flow:lobby_leave_teardown |
| 7041 | PracticeLobbyLaunch | PracticeLobbyLaunch | D+W | LobbyLifecycle | replay:game_flow:lobby_launch_allpick |
| 7046 | PracticeLobbySetDetails | PracticeLobbySetDetails | D+W | LobbyMutation | smoke:…set_details… |
| 7047 | PracticeLobbySetTeamSlot | PracticeLobbySetTeamSlot | D+W | LobbyMutation | smoke:…set_team_slot… |
| 7081 | PracticeLobbyKick | PracticeLobbyKick | D+W | LobbyMutation | replay:lobby_lifecycle:lobby_kick_player |
| 7149 | JoinBroadcastChannel | PracticeLobbyJoinBroadcastChannel | D+W | LobbyMutation | smoke:… |
| 7367 | UpdateBroadcastChannelInfo | LobbyUpdateBroadcastChannelInfo | D+W | LobbyMutation | smoke:… |
| 8054 | CloseBroadcastChannel | PracticeLobbyCloseBroadcastChannel | D+W | LobbyMutation | smoke:… |
| 7070 | CustomGameReadyUp | CustomGameReadyUp | D+W | LobbyLifecycle | smoke:custom_game_lifecycle… |
| 8052 | CustomGameStartedLoading | CustomGameStartedLoading | D+W | LobbyLifecycle | smoke:…8052… |
| 8053 | CustomGameFinishedLoading | CustomGameFinishedLoading | D+W | LobbyLifecycle | smoke:… |
| 7427 | Notifications | Notifications7427 | Direct | None | — |
| 4523 | UploadRate | UploadRate | Direct | None | — |
| 8879 | Rank | Rank | Direct | None | smoke:…rank…；template 仅 REGISTRY_DEFENSIVE 转调 |
| 7534 | ProfileCard | ProfileCard | Direct | None | — |
| 2581 | LookupAccountName | LookupAccountName | Direct | None | — |
| 7503 | EmoticonData | EmoticonData | Direct | None | — |
| 8095 | ConductScorecard | ConductScorecard | Direct | None | —；template 仅 REGISTRY_DEFENSIVE 转调 |
| 8800 | CoachingSummary | CoachingSummary | Direct | None | — |
| 7034 | Match runtime | Direct7034 | Direct | LobbyLifecycle | smoke:test_match_7034_host_showcase_repush_guard_marks_once |
| 2569 | EquipItems | EquipItems | Direct | LobbyMutation | smoke:test_inventory_equip_full_forward |
| 7035 | AbandonCurrentGame | AbandonCurrentGame | D+W | LobbyLifecycle | smoke:test_lobby_abandon… |
| 7004 | GameMatchSignOut | GameMatchSignOut | D+W | LobbyLifecycle | smoke:test_lobby_game_match_signout… |
| 8246 | DestroyLobby | DestroyLobby | D+W | LobbyLifecycle | smoke:test_lobby_destroy… |
| 4506 | LaunchAdvance | LaunchAdvance4506 | Direct | None | — |
| 5429 | TicketAuth | LaunchAdvanceTicketAuth | Direct | None | — |
| 8870 | LaunchMarker | LaunchMarker8870 | Direct | None | — |
| 4511 | LanServerAvailable | LanServerAvailable | Direct | LobbyLifecycle | smoke:test_misc_lan_server… |
| 4508 | ServerAssignment | ServerAssignment | Direct | LobbyLifecycle | smoke:test_misc_server_assignment… |
| 7273 | ChatMessage | ChatMessage | Direct | None | — |
| 7272 | LeaveChatChannel | LeaveChatChannel | D+W | LobbyMutation | smoke:test_chat_leave_postgame_channel_order |
| 1087 | AddSocket | AddSocket | Direct | None | — |
| 2571 | UnlockItemStyle | UnlockItemStyle | Direct | LobbyMutation | smoke:test_inventory_unlock_style_with_consumable |
| 2577 | SetItemStyle | SetItemStyle | Direct | LobbyMutation | smoke:test_inventory_set_style_success |
| 8727 | minimal → 8728 | MinimalVarint8727 | Direct | None | smoke:test_misc_minimal_varint_success |
| 8886 | minimal → 8887 | MinimalVarint8886 | Direct | None | smoke:test_misc_minimal_varint_success |
| 8793 | minimal → 8794 | MinimalVarint8793 | Direct | None | smoke:test_misc_minimal_varint_success |
| 7450 | BatchPlayerResources | BatchPlayerResources | Direct | None | — |
| 28 | CacheSubscriptionRefresh | CacheSubscriptionRefresh | Direct | None | — |
| 7072 | LeaverDetected | LeaverDetected | Direct | LobbyLifecycle | smoke:test_misc_leaver_detected_publishes_before_details |
| 7381 | SignOutPermission | SignOutPermission | Direct | None | — |
| 7082 | SubmitPlayerReportV2 | SubmitPlayerReportV2 | Direct | None | — |
| 8009 | FindTopSourceTVGames | FindTopSourceTVGames | D+W | LobbyRead | — |
| 7091 | WatchGame | WatchGame | D+W | LobbyRead | — |

Adapter 形态：`self->GBE_Handle…`（仍是 GC 成员，非独立服务）。

---

## 2. 仍留在 if/fallback 的路径（有意保留）

### Direct（`GBE_HandleDotaDirectPostLoginRequest`）

| emsg | 归属 | 行为 | 原因 |
|------|------|------|------|
| 8744 | CONDITIONAL_PROBE | 经 direct conditional fallback helper 仅 debug log，再落 template | 观察探针；生产回复归 TEMPLATE_ONLY |
| GamesPlayedWithDataBlob (5410) | CONDITIONAL_CONSUME | 经 direct conditional fallback helper 条件 consume + return true | 依赖 `GBE_ShouldTrackDotaPracticeLobbyLateSteamChain()` |
| AuthList (5432) | CONDITIONAL_CONSUME | 同上 | 同上 |
| （其余 miss） | TEMPLATE_ONLY ladder | `GBE_HandleDotaTemplateReplayRequest` | 白名单 catch-all |

### Wrapped（`GBE_HandleDotaWrappedPostLoginRequest`）

| emsg | 归属 | 行为 | 原因 |
|------|------|------|------|
| registry hit | REGISTRY | `GBE_DispatchDotaPostLoginRequest` | 主路径 |
| （未注册 miss） | HARD_MISS | 经 wrapped hard miss helper log + return false | B4：删除误落到 SetTeamSlot 的 dead fallback |

---

## 3. Hello / Welcome pipeline（显式 handler，非 post-login registry）

| emsg | 名 | 行为 | 锚点 |
|------|-----|------|------|
| 4007 | ServerHello | `GBE_HandleDotaServerHelloRequest`：解析 → ServerWelcome + 可选 CacheSubscribed | `gbe_dota_welcome_coordinator.cpp` |
| 4006 | ClientHello | `GBE_HandleDotaClientHelloRequest(direct)`：ClientWelcome + top custom + login sync | 同上 |
| 5452 | ClientToGC | 先尝试 Hello extract → ClientHello handler；否则 wrapped post-login | `handle_dota_client_message` 路由薄壳 |

说明：Hello 不进 kTable（envelope/语义与 post-login 不同）；完成定义是入口可点名、实现可单测路径清晰。

---

## 4. template_replay（catch-all 风险区）

源：`dll/gbe_dota_template_replay_handlers.cpp` switch。
**语义：** registry 与条件 fallback 皆未处理时的兜底。
**归属标签：** `TEMPLATE_ONLY`（本轨生产 owner）| `REGISTRY_DEFENSIVE`（registry 已有，仅转调，防 dispatch 顺序漂移）。

| emsg | 归属 | 备注 |
|------|------|------|
| 2536, 2617, 8137, 8673, 7197 | TEMPLATE_ONLY | canned 字节模板 |
| 8729, 8744, 8330 | TEMPLATE_ONLY | canned hex；8744 与 direct 观察 log 叠加后进入 |
| 8676 | TEMPLATE_ONLY | canned + unsolicited 8678 followup |
| 7387 | TEMPLATE_ONLY | synthetic minimal 7388 |
| 8078, 8853, 9023 | TEMPLATE_ONLY | canned 字节模板 |
| 8218 | TEMPLATE_ONLY | synthetic tip success |
| 8879 | REGISTRY_DEFENSIVE | → `GBE_HandleDotaRankRequest`（已删 divergent canned） |
| 8095 | REGISTRY_DEFENSIVE | → `GBE_HandleDotaConductScorecardRequest`（已删 suppress 分支） |
| 8020 | TEMPLATE_ONLY | CustomGameInfoRequest synthetic |
| 7466 | TEMPLATE_ONLY | JoinableCustomGameModesRequest synthetic |
| 7468 | TEMPLATE_ONLY | JoinableCustomLobbiesRequest synthetic |
| 8009 | REGISTRY_DEFENSIVE | → FindTopSourceTVGames handler |
| 7091 | REGISTRY_DEFENSIVE | → WatchGame handler |
| 7073 | TEMPLATE_ONLY | SpectateFriendGame synthetic |
| 8209, 8260 | TEMPLATE_ONLY | claim event action synthetic |
| 2510 | TEMPLATE_ONLY | StorePurchaseInit + inventory grant |
| 1092, 1025, 2574, 2576 | TEMPLATE_ONLY | crate/use/unlock/unpack synthetic |

硬规则：禁止无标注新增 case；新增默认 template-only 须先改本表。

---

## 5. 解析层职责（B4 评估结论：分责保留，禁止再叠第三套）

| 组件 | 文件 | 生产职责 | 状态 |
|------|------|----------|------|
| gc_router | `gbe_dota_gc_router.*` | **wrapped post-login 入站**（`extract_wrapped_post_login_request`）+ 出站 `build_outbound_message`；产出 `DotaGcRequestContext` 供 registry | 权威入口 |
| request_router | `gbe_dota_request_router.h` | **direct 帧解析**（`GBE_ParseDirectProtoContext`）+ 模板内层抽取 `GBE_ExtractWrappedClientFromGCPayload` | 权威入口 |
| request_router | `GBE_ExtractWrappedDotaDirectContext` | 与 gc_router wrapped 抽取语义重叠 | **LEGACY_UNUSED**（无生产 call site） |

决策（B4）：
1. 不合并实现：direct 需要 protobuf header 对象；wrapped 需要 session field 字符串；合并收益低、回归面大。
2. 生产路径禁止新增对 `GBE_ExtractWrappedDotaDirectContext` 的调用；wrapped 一律走 gc_router。
3. 若未来合并，先加 golden parse fixture，再删 LEGACY_UNUSED。

---

## 6. 维护检查清单

- [ ] 改 `kTable` → 更新 §1 与 smoke `view.size`
- [ ] 删/迁 fallback → 更新 §2
- [ ] 改 Hello → 更新 §3
- [ ] 改 template switch → 更新 §4
- [ ] PR 描述写明 emsg + 原入口 + 新入口
