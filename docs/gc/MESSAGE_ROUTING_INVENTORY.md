# 消息路由清单（四轨真相表）

> 源码扫描日期：2026-07-13（A2 后：if-chain 主体已迁 registry）。改入口必须同步更新本表。
> 分发顺序（post-login）：**registry → 条件/观察 fallback → template_replay**。
> Hello / ServerHello 在 `handle_dota_client_message` 内联，不进 registry。

## 0. 分发骨架

| 阶段 | 位置 | 说明 |
|------|------|------|
| 入口 | `Steam_Game_Coordinator::SendMessage_` → `handle_dota_client_message` | `dll/steam_game_coordinator.cpp` |
| ServerHello | 内联 | masked `GBE_kEMsgGCServerHello` (4007) |
| ClientHello | 内联 welcome | masked `GBE_kEMsgGCClientHello` (4006) |
| ClientToGC 非 Hello | `GBE_HandleDotaWrappedPostLoginRequest` | 先 registry，再 8009/7091 |
| 其它 direct | `GBE_HandleDotaDirectPostLoginRequest` | registry → 条件路径 → template |
| Registry 表 | `GBE_ProductionDotaHandlerRegistry` | `dll/gbe_dota_post_login_dispatcher.cpp`（**50** 条） |
| Template | `GBE_HandleDotaTemplateReplayRequest` | `dll/gbe_dota_template_replay_handlers.cpp` |

**硬规则：** 新增 emsg 只加 registry 行；禁止新增 if-chain / 默认 template case（template-only 白名单除外）。

---

## 1. Registry（生产 kTable，50）

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
| 8879 | Rank | Rank | Direct | None | —（template 亦有 case） |
| 7534 | ProfileCard | ProfileCard | Direct | None | — |
| 2581 | LookupAccountName | LookupAccountName | Direct | None | — |
| 7503 | EmoticonData | EmoticonData | Direct | None | — |
| 8095 | ConductScorecard | ConductScorecard | Direct | None | —（template 亦有 case） |
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

Adapter 形态：`self->GBE_Handle…`（仍是 GC 成员，非独立服务）。

---

## 2. 仍留在 if/fallback 的路径（有意保留）

### Direct（`GBE_HandleDotaDirectPostLoginRequest`）

| emsg | 行为 | 原因 |
|------|------|------|
| 8744 | 仅 debug log，再落 template | 观察探针，非 handler |
| GamesPlayedWithDataBlob (5410) | 条件 consume + return true | 依赖 `GBE_ShouldTrackDotaPracticeLobbyLateSteamChain()` |
| AuthList (5432) | 同上 | 同上 |
| （其余 miss） | `GBE_HandleDotaTemplateReplayRequest` | template catch-all |

### Wrapped（`GBE_HandleDotaWrappedPostLoginRequest`）

| emsg | 行为 | 目标 |
|------|------|------|
| 8009 | FindTopSourceTVGames 内联实现 | 后续可抽 handler + registry |
| 7091 | WatchGame 内联实现 | 同上 |
| （末尾） | 误落到 SetTeamSlot 的 dead fallback | 7047 已在 registry；正常不应到达 |

---

## 3. Hello 内联（非 registry）

| emsg | 名 | 行为 | 锚点 |
|------|-----|------|------|
| 4007 | ServerHello | 解析 context → ServerWelcome + 可选 CacheSubscribed | `steam_game_coordinator.cpp` handle_dota ~1288+ |
| 4006 | ClientHello | ClientWelcome 合成 | 同文件 ~1416+ |
| 5452 | ClientToGC | Hello 或 wrapped post-login | ~1422+ |

目标（Phase B）：迁 welcome pipeline / 显式注册。

---

## 4. template_replay（catch-all 风险区）

源：`dll/gbe_dota_template_replay_handlers.cpp` switch。
**语义：** registry 与条件 fallback 皆未处理时的兜底。

| emsg | 备注 |
|------|------|
| 2536, 2617, 8137, 8673, 7197, 8729 | 模板回放 |
| 8744 | 与 direct 观察 log 叠加后进入 |
| 8330, 8676, 7387, 8078, 8853, 9023, 8218 | 模板回放 |
| 8879, 8095 | **registry 已有**；确认是否 dead template 分支 |
| 8020 | CustomGameInfoRequest |
| 7466 | JoinableCustomGameModesRequest |
| 7468 | JoinableCustomLobbiesRequest |
| 8009 | FindTopSourceTVGames（direct 可走 template；wrapped 有专用 if） |
| 7073, 7091, 8209, 2510, 1092, 1025, 2574, 2576, 8260 | 模板回放 |

目标：每条标 `template-only` 或迁实现/registry；禁止无标注新增 case。

---

## 5. 解析层重复（Phase B 合并）

| 组件 | 文件 | 职责 |
|------|------|------|
| gc_router | `gbe_dota_gc_router.*` | envelope 解析 / 出站封装 |
| request_router | `gbe_dota_request_router.h` | 另一套 direct/wrapped 解析（inline） |

---

## 6. 维护检查清单

- [ ] 改 `kTable` → 更新 §1 与 smoke `view.size`
- [ ] 删/迁 fallback → 更新 §2
- [ ] 改 Hello → 更新 §3
- [ ] 改 template switch → 更新 §4
- [ ] PR 描述写明 emsg + 原入口 + 新入口
