# Post-login 入口清单（registry vs if 兜底）

> 源：`dll/gbe_dota_post_login_dispatcher.cpp`、`dll/gbe_dota_post_login_handlers.cpp`  
> 分发：`GBE_DispatchDotaPostLoginRequest` 命中 registry 则返回；否则走 if 链。

## 1. Registry 已表化（`kTable`）

| message_id | HandlerId | modes | lifecycle | fixture |
|------------|-----------|-------|-----------|---------|
| JoinChatChannel | JoinChatChannel | D+W | LobbyMutation | smoke:test_chat_join_channel |
| PracticeLobbyCreate | PracticeLobbyCreate | D+W | LobbyLifecycle | replay:lobby_lifecycle:lobby_create_with_passkey |
| LobbyList | LobbyList | D+W | LobbyRead | — |
| CustomLobbyList | CustomLobbyList | D+W | LobbyRead | — |
| FriendPracticeLobbyList | FriendPracticeLobbyList | D+W | LobbyRead | — |
| GCInviteToLobby | InviteToLobby | D+W | LobbyMutation | smoke:…invite… |
| GCLobbyInviteResponse | LobbyInviteResponse | D+W | LobbyMutation | smoke:…decline… |
| PracticeLobbyJoin | PracticeLobbyJoin | D+W | LobbyLifecycle | replay:lobby_lifecycle:lobby_join_by_id |
| PracticeLobbyLeave | PracticeLobbyLeave | D+W | LobbyLifecycle | replay:game_flow:lobby_leave_teardown |
| PracticeLobbyLaunch | PracticeLobbyLaunch | D+W | LobbyLifecycle | replay:game_flow:lobby_launch_allpick |
| PracticeLobbySetDetails | PracticeLobbySetDetails | D+W | LobbyMutation | smoke:…set_details… |
| PracticeLobbySetTeamSlot | PracticeLobbySetTeamSlot | D+W | LobbyMutation | smoke:…set_team_slot… |
| PracticeLobbyKick | PracticeLobbyKick | D+W | LobbyMutation | replay:lobby_lifecycle:lobby_kick_player |
| PracticeLobbyJoinBroadcastChannel | … | D+W | LobbyMutation | smoke:… |
| LobbyUpdateBroadcastChannelInfo | … | D+W | LobbyMutation | smoke:… |
| PracticeLobbyCloseBroadcastChannel | … | D+W | LobbyMutation | smoke:… |
| 7070 | CustomGameReadyUp | D+W | LobbyLifecycle | smoke:custom_game_lifecycle… |
| 8052 | CustomGameStartedLoading | D+W | LobbyLifecycle | smoke:…8052… |
| 8053 | CustomGameFinishedLoading | D+W | LobbyLifecycle | smoke:… |
| 7427 | Notifications7427 | Direct | None | — |
| 4523 | UploadRate | Direct | None | — |
| 8879 | Rank | Direct | None | — |
| 7534 | ProfileCard | Direct | None | — |
| 2581 | LookupAccountName | Direct | None | — |
| 7503 | EmoticonData | Direct | None | — |
| 8095 | ConductScorecard | Direct | None | — |
| 8800 | CoachingSummary | Direct | None | — |

约 27 条。High-risk 在 registry 内多数带 fixture 标签（文档链接，非自动 dual-GC）。

## 2. if 兜底（`gbe_dota_post_login_handlers.cpp` direct 路径）

| emsg / 常量 | handler | high-risk | 备注 |
|-------------|---------|-----------|------|
| ChatMessage | GBE_HandleDotaChatMessageRequest | 中 | 未进 registry |
| LeaveChatChannel | GBE_HandleDotaLeaveChatChannelRequest | 中 | 未进 registry |
| DestroyLobby | GBE_HandleDotaDestroyLobbyRequest | **高** | 阶段 C 优先迁入 |
| AddSocket | GBE_HandleDotaAddSocketRequest | 中 | |
| UnlockItemStyle / SetItemStyle | style handlers | 中 | |
| 8727 / 8886 / 8793 / 7450 | template / misc | 中 | |
| **7034** | GBE_HandleDotaDirect7034Request | **高** | host 热路径 |
| 8744 | launch/misc | 中 | |
| CacheSubscriptionRefresh | … | 中 | |
| **AbandonCurrentGame** | GBE_HandleDotaAbandonCurrentGameRequest | **高** | |
| LeaverDetected | … | 中 | |
| SignOutPermission | … | 中 | |
| **GameMatchSignOut** | GBE_HandleDotaGameMatchSignOutRequest | **高** | |
| SubmitPlayerReportV2 | … | 低 | |
| 4506 / SteamTicketAuth / 8870 / 4511 / 4508 | launch 标记链 | **高** | |
| GamesPlayedWithDataBlob / AuthList | late steam chain | 中 | 条件触发 |
| **2569** | GBE_HandleDotaEquipItemsRequest | **高** | host equip |

Wrapped 路径另有 abandon / signout / destroy 等平行 if（约 614 行后）。

## 3. 阶段 C 迁移优先级

1. 7034  
2. 2569  
3. Abandon / SignOut / Destroy  
4. 8870 / 4511 / 4508 等 launch 标记  
5. 其余 chat/style/template  
