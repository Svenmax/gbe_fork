# Local lobby 使用清单（C1 审计）

> 对照代码日期：2026-07-25。描述 `GBE_local_lobby` 与 shared Store 的生产写路径。
> 冲突以本文件 + `HOST_AUTHORITY.md` + 代码为准。

## 1. 角色

| 对象 | 角色 |
|------|------|
| `GBE_local_lobby` | 本 GC 工作副本；handler/coordinator 可就地改字段 |
| shared Store | 跨 client/server GC 快照；**生产写只经 generation 门控** |
| `GBE_PublishSharedDotaLobbyState` | local → shared 唯一 publish 门面 |
| `GBE_SyncCapturedDotaLobbyState` | 已完成 Local capture 后的显式 host-only publish 边界 |
| `GBE_RestoreSharedDotaLobbyState` / adopt | shared → local 恢复 |
| `GBE_ClearDotaLobbyRuntimeState` | local + shared compare_clear + last-launch 全清 |

## 2. Shared Store 写纪律（已冻结）

| API | 允许场景 |
|-----|----------|
| `publish_if_generation_current_or_newer` | 仅 `GBE_PublishSharedDotaLobbyState` 内部 |
| `compare_update` / `compare_clear` | 门控增量 / runtime clear |
| `publish` / `update` / 裸 `clear` | **仅 offline 测试 bootstrap**；`audit_store_write_discipline` 禁止生产 dll |

生产 publish 调用面（均经 `GBE_PublishSharedDotaLobbyState`）：chat / create / join / slot / launch / lifecycle / match 7034 / misc 4511·7072 / connection / flow steam_auth / snapshot owner_transfer / member coordinator / custom_game lifecycle / queued_state 等。新增 shared 写必须挂到该门面或 compare_*。

## 3. Local 全量赋值（`GBE_local_lobby = …`）

| 位置 | 意图 | 备注 |
|------|------|------|
| `lobby_create_handlers` | apply create plan | 经 `apply_create_lobby_state_plan()` 应用完整 lobby |
| `lobby_join_handlers` | apply join plan | 经 `apply_join_lobby_merge_plan()` 应用完整 lobby |
| `lobby_lifecycle_handlers` | apply launch_init plan | 经 `apply_launch_init_plan()` 应用完整 lobby |
| `lobby_launch_coordinator` | apply serversetup lobby | 经 `apply_custom_game_launch_serversetup_plan()` 应用完整 lobby |
| `lobby_slot_handlers` | kick 后回滚 `before_lobby` | 经 `apply_lobby_member_kick_snapshot()` 应用完整 lobby |
| `chat_handlers` | leave postgame 后 `{}` | 经 `clear_local_lobby()` 清空 Local，不 re-publish stale shared（见 shared-lobby-state-contract） |
| `steam_game_coordinator` | `GBE_ClearDotaLobbyRuntimeState` → `{}` | 经 `clear_local_lobby()` 清空 Local |
| `lobby_state_restore_coordinator` | adopt 失败/空 shared 时 `{}` | 经 `clear_local_lobby()` 清空 Local |
| `custom_game_lifecycle_coordinator` | 对侧 client restore 指针 | 经 `apply_client_lobby_restore_snapshot()` 应用完整 lobby |

规则：新增完整 Local 快照写入须有 plan/restore/clear 语义，并通过命名 state helper；生产 `.cpp` 中直接 `GBE_local_lobby = ...` 或 `GBE_local_lobby.<field> = ...` 写入由 audit 10d 禁止；禁止在 payload helper 里整结构覆盖。

## 4. Local 字段就地写（摘要）

- **state / game_state / launch_***：queued-state 路径先经 `compose_queued_lobby_state_apply_plan()` 计算，再由 `apply_queued_lobby_state_apply_plan()` 一次写入三字段；monotonic phase 推进经 `advance_launch_phase()`；generic lobby metadata 读取先经 `compose_generic_lobby_capture_plan()` 汇总 state、runtime identity、options 与 custom_game plan，再由 `apply_generic_lobby_capture_plan()` 一次应用，保留既有 state 的 launch 回退保护、runtime identity 的 launched LAN runtime 保护与空 raw key 跳过语义。shared restore 的 launch/runtime 与 identity 字段组统一经 `compose_source_aware_shared_runtime_restore_plan()` 和 `apply_source_aware_shared_runtime_restore_plan()`；同 generation 的 Local generic capture 保留字段组，其他 client observe 采用 shared snapshot，并保留 READYUP state 回退保护。
- **payload snapshot 消费**：cache template replay、cache payload、private lobby replay 与 26 details update 统一经 `GBE_CaptureCurrentDotaLobbySnapshotForPayload()` 取得纯 projection 快照。capture 显式区分同步 capture、无 shared restore capture 与纯 snapshot projection；纯模式从 Local 副本应用 generic plan、成员合并与 arcade slot normalization，保持 Local、generic metadata、shared Store 与 owner repair/adopt/hero apply 不变。private replay 的无 shared restore 语义由纯 projection 保持。
- **generic metadata capture 模式契约**：7009 是唯一 host sync：无 shared restore 的 Local capture 后，`GBE_SyncCapturedDotaLobbyState(..., is_server)` 仅发布 host 快照，顺序为 capture、host publish、7010 response。成员变更（含 previous slots）、7034 custom runtime member refresh 与 launch-state push 为 client observe：更新目标 Local 工作副本，不由 capture 触发 publish。cache template、cache payload、private replay 与 26 details 通过 `GBE_CaptureCurrentDotaLobbySnapshotForPayload()` 走 pure projection：只修改 payload 副本，保持 Local、shared Store、owner repair/adopt 不变。
- **Local clear**：runtime clear、postgame 7272 stale shared cleanup 与 recover generation exhausted 的 Local 清空经 `clear_local_lobby()`，shared Store clear、generic lobby leave、launch-state clear 与 reconnect context 清理顺序仍由对应 coordinator/handler 管理。
- **members / chat / broadcast**：slot、chat、member_coordinator；7009 join 与 7272 leave 的 chat channel 字段组经 `apply_chat_channel()` / `clear_chat_channel()` 写入 Local，host sync、7010/7014 push 与 postgame tombstone 顺序仍由 chat handler 管理；7149 join、7367 update 与 8054 close 的 broadcast channel 字段组经 `apply_broadcast_channel()` / `patch_broadcast_channel()` / `clear_broadcast_channel()` 写入 Local，publish、details update 与 ack 顺序仍由 chat handler 管理。
- **kick member snapshot apply**：7081 generic kick 成功后的 Local 快照写回经 `apply_lobby_member_kick_snapshot()`，generic kick 调用、shared publish、details update 与日志顺序仍由 slot handler 管理。
- **create state apply**：7038 create plan 的完整 Local 快照写回经 `apply_create_lobby_state_plan()`，generation 写入、custom game normalize、arcade slot normalize、reconnect context 与后续 create actions 顺序仍由 create handler 管理。
- **join merge apply**：7044 join plan 的完整 Local 快照写回经 `apply_join_lobby_merge_plan()`，generation advance/write、generic lobby join/settings sync、local member data、publish 与 response 顺序仍由 join handler 管理。
- **launch init apply**：7041 launch init plan 的完整 Local 快照写回经 `apply_launch_init_plan()`，LaunchPeripheralReset、shared publish、custom game setup flow、rich presence 与 persona state 顺序仍由 lifecycle handler 管理。
- **custom game launch serversetup apply**：7041 custom game setup flow 的 SERVERSETUP Local 快照写回经 `apply_custom_game_launch_serversetup_plan()`，READYUP details push、shared publish、SERVERSETUP details push、launch phase mark 与 steam-auth ack queue 顺序仍由 launch coordinator 管理。
- **client lobby restore snapshot apply**：custom game lifecycle cross-GC mirror 的 client Local 快照恢复经 `apply_client_lobby_restore_snapshot()`，client target guard、active/lobby_id guard、launch peripheral reset 与 last launch state clear 顺序仍由 custom game lifecycle coordinator 管理。
- **owner_hero_id**：只允许 `HOST_AUTHORITY` 列出的 Apply/adopt/publish 路径（禁止 match/inventory 旁路 `=`）。
- **server_id / connect / match_id / game_start_time / room_name restore/apply**：shared-to-local restore 由 source-aware plan 一次应用；空 connect、零 match_id、零 server_id 与零 start_time 保留 Local 值，room_name 继续允许 shared 清空。4508 runtime connect 的 Local 写入经 `apply_runtime_connect()`，空 connect 与相同 connect 保留既有字段，LAN preserve guard 与 shared Store compare_update 仍由 post-login handler 管理；generic metadata publish 的 connect/server_id Local 写入经 `apply_runtime_metadata()`，shared Store compare_update、generic metadata publish 与日志顺序仍由 publish coordinator 管理；recover coordinator 的 derived server_id 写入经 `apply_lobby_server_id()`，derived guard、shared Store compare_update 与 diagnostics 顺序仍由 recover coordinator 管理。
- **owner_connected / owner_team / owner_slot restore**：shared-to-local restore 经 `restore_lobby_owner_connected()`、`restore_lobby_owner_team()` 与 `restore_lobby_owner_slot()` 返回变更；launch member connection owner 分支与 connection lifecycle owner connect/disconnect 分支经 `apply_lobby_owner_connected()` 写入 Local 并保持 changed 聚合、suppress guard 与 postgame publish suppression 语义；7034 draft owner team/slot 与 7047 set team slot owner 分支经 `apply_lobby_owner_team()` 与 `apply_lobby_owner_slot()` 写入 Local，draft owner slot 零值 guard、7047 member update、bot difficulty 与 publish 顺序仍在 handler。
- **owner_name local apply**：generic lobby owner adoption 的 owner_name 写入经 `apply_lobby_owner_name()`，owner adoption 决策、local owner publish、metadata publish 与日志顺序仍由 member coordinator 管理。
- **generic lobby observation flags**：kick/adoption 观察标记经 `note_generic_lobby_local_member_seen()`、`mark_generic_lobby_waiting_join_confirmation_logged()`、`mark_generic_lobby_kicked_suppressed_logged()` 与 `mark_generic_lobby_owner_adoption_suppressed_logged()` 写入 Local，一次性日志、kick detection、owner adoption 决策与 push/reset 顺序仍由 member coordinator 管理。
- **members restore**：shared-to-local runtime restore 的 members 字段组经 `restore_lobby_members()` 写入 Local，changed 聚合、sync settings、rich presence replay 与 login sync 顺序仍由 restore coordinator 管理。
- **options restore**：shared-to-local restore 的 game_mode/server_region/lan/ping/allow_cheats/fill_with_bots/allow_spectating/pass_key/visibility/bot_* 经 `compose_shared_lobby_options_restore_plan()` 与 `apply_shared_lobby_options_restore_plan()` 返回字段组变更。
- **details update local apply**：7046 set details 的 options/custom_game 字段组经 `apply_lobby_details_update()` 写入 Local，lobby id mismatch 日志、custom game normalize、arcade slot normalize、details push 与 diagnostics 顺序仍由 create handler 管理。
- **cache restore**：shared-to-local restore 的 has_cache_version/cache_version/has_cache_service_id/cache_service_id/cache_service_list/has_cache_sync_version/cache_sync_version 经 `compose_shared_lobby_cache_restore_plan()` 与 `apply_shared_lobby_cache_restore_plan()` 返回字段组变更。
- **cache subscription metadata local apply**：`GBE_RecordDotaLobbyCacheSubscriptionState(...)` 解析后的 cache metadata 字段组经 `apply_cache_subscription_metadata()` 写入 Local，owner SOID guard、日志、summary 与 publish 顺序仍由 publish coordinator 管理。
- **custom_game restore**：shared-to-local restore 经 `restore_lobby_custom_game()` 返回变更；比较复用 `custom_game_details_equal()`。
- **custom game loading metadata local apply**：8052 started loading 的 custom_game_id / start_time 经 `apply_custom_game_loading_metadata()` 写入 Local，零值输入保留既有字段，launch setup 计算与 lifecycle decision 顺序仍由 lifecycle handler 管理。
- **bot difficulty team local apply**：7047 set team slot 的 radiant/dire bot difficulty 经 `apply_lobby_bot_difficulty_for_team()` 写入 Local，bot team 推导与 request guard 仍由 slot handler 管理。
- **generation / generic_lobby_id restore/apply**：shared-to-local restore 经 `restore_lobby_generation()` 与 `restore_lobby_generic_lobby_id()` 返回变更，generation restore 复用 `apply_lobby_generation()`，generic_lobby_id restore 复用 `apply_lobby_generic_lobby_id()`；create/join/runtime reset/lifecycle clear/recover 的 generation 写入经同一 generation apply helper，generation advance 与 counter 更新顺序仍由各 Coordinator/handler 管理；7038 create、7040 leave fallback 与 leave-generic clear 的 generic_lobby_id 写入经同一 apply helper，create action、settings sync 与 leave cleanup 顺序仍由各 Coordinator/handler 管理。
- **SourceTV metadata local apply**：4508 game server info 的 `tv_secret_code` / `tv_port` 仅通过 `apply_source_tv_metadata()` 写入 Local；零值输入保留既有字段，publish 顺序仍由 post-login handler 原 guard 管理。
- **launch_steam_auth_***：仅 steam-auth ack 路径通过 `compose_steam_auth_ack_launch_plan()` 与 `apply_steam_auth_ack_launch_plan()` 一次写入 CRC、message sequence 与 ack 标记，再 publish shared state。
- **launch_4511_seen**：仅匹配 lobby 的 4511 通知通过 `mark_launch_4511_seen()` 幂等标记；首次变更才 publish shared state。
- **launch_4511_seen restore**：shared-to-local restore 通过 `restore_launch_4511_seen()` 返回字段变更；Coordinator 继续聚合 restore 的 `changed` 结果。
- **lifecycle state / game_state**：非 runtime-queue lifecycle 路径仅 `LobbyStateApply` action 通过 `apply_lifecycle_lobby_state()` 一次写入；`8052` runtime-queue 路径经 `LocalLifecyclePreWrite` action 在 queue / fallback publish 前一次写入 Local `state`、`game_state` 与 `launch_phase`；action 序列和后续 publish 继续由 lifecycle executor 管理。
- **postgame state / chat / cache**：仅 `PostGameLobbyStateApply` action 通过 `apply_postgame_lobby_state_plan()` 一次写入 state、chat 与 cache 清理字段组；executor 继续管理 action 序列与 publish。
- **postgame chat tombstone**：`PostGameLobbyStateApply` 同步写入 `postgame_chat_tombstone_active`、`postgame_chat_tombstone_channel_id` 与 `postgame_chat_tombstone_generation`，旧 `abandon_pre_postgame_chat_channel_id` 保持为兼容日志字段；old-channel 7272 与 7014 retrieval 通过 `postgame_chat_tombstone_matches()` 做 generation-scoped 匹配，当前 postgame channel leave 通过 `clear_postgame_chat_tombstone()` 清理。

## 5. Local/shared merge inventory（由 audit 保护）

| entrypoint | owner | 字段组 |
|------------|-------|--------|
| `compose_source_aware_shared_runtime_restore_plan` | `gbe_dota_lobby_state.cpp` | launch/runtime identity restore |
| `apply_source_aware_shared_runtime_restore_plan` | `gbe_dota_lobby_state.cpp` | launch/runtime identity restore |
| `compose_shared_lobby_options_restore_plan` | `gbe_dota_lobby_state.cpp` | options restore |
| `apply_shared_lobby_options_restore_plan` | `gbe_dota_lobby_state.cpp` | options restore |
| `compose_shared_lobby_cache_restore_plan` | `gbe_dota_lobby_state.cpp` | cache restore |
| `apply_shared_lobby_cache_restore_plan` | `gbe_dota_lobby_state.cpp` | cache restore |
| `restore_lobby_custom_game` | `gbe_dota_lobby_state.cpp` | custom_game restore |
| `restore_lobby_generation` | `gbe_dota_lobby_state.cpp` | generation restore |
| `restore_lobby_generic_lobby_id` | `gbe_dota_lobby_state.cpp` | generic_lobby_id restore |
| `restore_lobby_owner_connected` | `gbe_dota_lobby_state.cpp` | owner runtime restore |
| `restore_lobby_owner_team` | `gbe_dota_lobby_state.cpp` | owner runtime restore |
| `restore_lobby_owner_slot` | `gbe_dota_lobby_state.cpp` | owner runtime restore |
| `restore_launch_4511_seen` | `gbe_dota_lobby_state.cpp` | launch marker restore |

## 6. C1 结论

1. Shared 写路径已单一化到 generation 门控门面；无需本轮改 Store API。
2. Local 仍是广泛工作副本；queued-state、monotonic launch phase、generic capture state/identity/options/custom_game、source-aware shared launch/runtime identity restore、details update local apply、members restore、owner_name local apply、generic lobby observation flags、local clear、kick member snapshot apply、create state apply、join merge apply、launch init apply、custom game launch serversetup apply、client lobby restore snapshot apply、server_id local apply、runtime metadata local apply、4508 runtime connect apply、SourceTV metadata local apply、owner_connected local apply、owner connection lifecycle local apply、owner_team/owner_slot local apply、owner team/slot slot-handler local apply、bot difficulty team local apply、custom game loading metadata local apply、chat channel local apply/clear、broadcast channel local apply/patch/clear、cache subscription metadata local apply、steam-auth 元数据、4511 标记/restore、owner/options/cache/custom_game/generation/generic_lobby_id restore/apply、generation local apply、lifecycle/postgame state apply、8052 lifecycle pre-write 与 postgame chat tombstone 已采用纯 apply/action/helper 边界。generic capture 的 host sync、client observe 与 pure projection 调用面由 `audit_generic_metadata_capture_modes` 回归保护。
3. 双轨（local + shared）风险仍在 CURRENT；本清单只冻结入口，不声明状态单一化完成。

## 7. 停手

- 不为美观搬 `GBE_local_lobby` 字段访问。
- 不改 host 权威字段语义（见 HOST_AUTHORITY）。
- 不在本轮引入 CompositionRoot 生产装配。
