# GC 协议入口与行为矩阵

## 目的

本文件从生产注册表提取 Dota post-login 请求的路由信息，用于评估协议更新影响范围、测试覆盖与重构风险。注册表是本矩阵的事实来源：`dll/gbe_dota_post_login_dispatcher.cpp:62-115`。

通道含义：`Direct` 为直接 Dota GC 请求；`Wrapped` 为外层 post-login 封装请求。标为“转发”的条目会保留 wrapped session field，规则由 `SessionPolicy::ForwardWrappedSession` 决定。

## 路由约束

- Dispatcher 仅处理 `context.valid` 的请求，见 `dll/gbe_dota_post_login_dispatcher.cpp:119-132`。
- 查找键由 `inner_emsg` 与 request path 构成，见 `dll/gbe_dota_handler_registry.h:138-150`。
- 同一 message ID 在同一通道只能有一个注册 entry，见 `dll/gbe_dota_handler_registry.h:152-167`。
- `LobbyMutation` 与 `LobbyLifecycle` 归类为高风险入口，并要求在注册表中有 fixture 名称，见 `dll/gbe_dota_handler_registry.h:128-175`。

## Lobby 与生命周期入口

| Message | 通道 | Handler | 风险类别 | Wrapped session | 已登记验证 |
| --- | --- | --- | --- | --- |
| `GBE_kDotaJoinChatChannel` | Direct, Wrapped | `JoinChatChannel` | LobbyMutation | 转发 | `test_chat_join_channel` |
| `GBE_kDotaPracticeLobbyCreate` | Direct, Wrapped | `PracticeLobbyCreate` | LobbyLifecycle | 转发 | `lobby_create_with_passkey` |
| `GBE_kDotaLobbyList` | Direct, Wrapped | `LobbyList` | LobbyRead | 转发 | 未登记 |
| `GBE_kDotaCustomLobbyListRequest` | Direct, Wrapped | `CustomLobbyList` | LobbyRead | 转发 | 未登记 |
| `GBE_kDotaFriendPracticeLobbyListRequest` | Direct, Wrapped | `FriendPracticeLobbyList` | LobbyRead | 转发 | 未登记 |
| `GBE_kGCInviteToLobby` | Direct, Wrapped | `InviteToLobby` | LobbyMutation | 转发 | wrapped response preserve |
| `GBE_kGCLobbyInviteResponse` | Direct, Wrapped | `LobbyInviteResponse` | LobbyMutation | 转发 | decline remove/unsubscribe |
| `GBE_kDotaPracticeLobbyJoin` | Direct, Wrapped | `PracticeLobbyJoin` | LobbyLifecycle | 转发 | `lobby_join_by_id` |
| `GBE_kDotaPracticeLobbyLeave` | Direct, Wrapped | `PracticeLobbyLeave` | LobbyLifecycle | 转发 | `lobby_leave_teardown` |
| `GBE_kDotaPracticeLobbyLaunch` | Direct, Wrapped | `PracticeLobbyLaunch` | LobbyLifecycle | 转发 | `lobby_launch_allpick` |
| `GBE_kDotaPracticeLobbySetDetails` | Direct, Wrapped | `PracticeLobbySetDetails` | LobbyMutation | 转发 | mutate/publish/details order |
| `GBE_kDotaPracticeLobbySetTeamSlot` | Direct, Wrapped | `PracticeLobbySetTeamSlot` | LobbyMutation | 转发 | publish/details/ack order |
| `GBE_kDotaPracticeLobbyKick` | Direct, Wrapped | `PracticeLobbyKick` | LobbyMutation | 转发 | `lobby_kick_player` |
| `GBE_kDotaPracticeLobbyJoinBroadcastChannel` | Direct, Wrapped | `PracticeLobbyJoinBroadcastChannel` | LobbyMutation | 转发 | publish/details/ack order |
| `GBE_kDotaLobbyUpdateBroadcastChannelInfo` | Direct, Wrapped | `LobbyUpdateBroadcastChannelInfo` | LobbyMutation | 转发 | publish/details order |
| `GBE_kDotaPracticeLobbyCloseBroadcastChannel` | Direct, Wrapped | `PracticeLobbyCloseBroadcastChannel` | LobbyMutation | 转发 | publish/details order |
| `7070` | Direct, Wrapped | `CustomGameReadyUp` | LobbyLifecycle | 转发 | direct/wrapped action equivalence |
| `8052` | Direct, Wrapped | `CustomGameStartedLoading` | LobbyLifecycle | 转发 | direct/wrapped action equivalence |
| `8053` | Direct, Wrapped | `CustomGameFinishedLoading` | LobbyLifecycle | 转发 | direct/wrapped action equivalence |
| `7034` | Direct | `Direct7034` | LobbyLifecycle | 忽略 | host wearable repush guard |
| `GBE_kDotaAbandonCurrentGame` | Direct, Wrapped | `AbandonCurrentGame` | LobbyLifecycle | 转发 | abandon disconnect queue |
| `GBE_kDotaGameMatchSignOut` | Direct, Wrapped | `GameMatchSignOut` | LobbyLifecycle | 转发 | signout/postgame sequence |
| `GBE_kDotaDestroyLobbyRequest` | Direct, Wrapped | `DestroyLobby` | LobbyLifecycle | 转发 | teardown sequence |
| `4511` | Direct | `LanServerAvailable` | LobbyLifecycle | 忽略 | matching lobby publish once |
| `4508` | Direct | `ServerAssignment` | LobbyLifecycle | 忽略 | handler smoke test |
| `GBE_kDotaLeaveChatChannel` | Direct, Wrapped | `LeaveChatChannel` | LobbyMutation | 转发 | postgame leave order |
| `GBE_kDotaLeaverDetected` | Direct | `LeaverDetected` | LobbyLifecycle | 忽略 | publish before details |

## 库存、配置与最小响应入口

| Message | 通道 | Handler | 风险类别 | 已登记验证 |
| --- | --- | --- | --- | --- |
| `2569` | Direct | `EquipItems` | LobbyMutation | full forward |
| `GBE_kDotaUnlockItemStyle` | Direct | `UnlockItemStyle` | LobbyMutation | consumable unlock |
| `GBE_kDotaSetItemStyle` | Direct | `SetItemStyle` | LobbyMutation | set style success |
| `7427` | Direct | `Notifications7427` | None | 未登记 |
| `4523` | Direct | `UploadRate` | None | 未登记 |
| `8879` | Direct | `Rank` | None | 未登记 |
| `7534` | Direct | `ProfileCard` | None | 未登记 |
| `2581` | Direct | `LookupAccountName` | None | 未登记 |
| `7503` | Direct | `EmoticonData` | None | 未登记 |
| `8095` | Direct | `ConductScorecard` | None | 未登记 |
| `8800` | Direct | `CoachingSummary` | None | 未登记 |
| `4506` | Direct | `LaunchAdvance4506` | None | 未登记 |
| `5429` | Direct | `LaunchAdvanceTicketAuth` | None | 未登记 |
| `8870` | Direct | `LaunchMarker8870` | None | 未登记 |
| `GBE_kDotaChatMessage` | Direct | `ChatMessage` | None | 未登记 |
| `GBE_kDotaAddSocket` | Direct | `AddSocket` | None | 未登记 |
| `8727` | Direct | `MinimalVarint8727` | None | minimal success |
| `8886` | Direct | `MinimalVarint8886` | None | minimal success |
| `8793` | Direct | `MinimalVarint8793` | None | minimal success |
| `7450` | Direct | `BatchPlayerResources` | None | 未登记 |
| `GBE_kDotaCacheSubscriptionRefresh` | Direct | `CacheSubscriptionRefresh` | None | 未登记 |
| `GBE_kDotaGameMatchSignOutPermissionRequest` | Direct | `SignOutPermission` | None | 未登记 |
| `GBE_kDotaSubmitPlayerReportV2` | Direct | `SubmitPlayerReportV2` | None | 未登记 |
| `GBE_kDotaFindTopSourceTVGames` | Direct, Wrapped | `FindTopSourceTVGames` | LobbyRead | 未登记 |
| `7091` | Direct, Wrapped | `WatchGame` | LobbyRead | 未登记 |

## 升级与重构检查

- 新 message ID 先加入注册表，再补充 direct/wrapped 通道、session policy、lifecycle class 和测试 fixture。
- 高风险入口至少覆盖成功路径、拒绝路径、generation 失效和副作用顺序。
- Direct 与 Wrapped 均受支持的生命周期入口需要验证响应语义和状态变化等价。
- 变更 `DotaGcRequestContext` 解析时，回放 wrapped outer header、inner header、job ID 和 session field。
- 注册表存在 fixture 名称只代表离线验证入口已登记；生产二进制消息兼容性仍需以真实捕获样本验证。
