# Local lobby 使用清单（C1 审计）

> 对照代码日期：2026-07-25。描述 `GBE_local_lobby` 与 shared Store 的生产写路径。
> 冲突以本文件 + `HOST_AUTHORITY.md` + 代码为准。

## 1. 角色

| 对象 | 角色 |
|------|------|
| `GBE_local_lobby` | 本 GC 工作副本；handler/coordinator 可就地改字段 |
| shared Store | 跨 client/server GC 快照；**生产写只经 generation 门控** |
| `GBE_PublishSharedDotaLobbyState` | local → shared 唯一 publish 门面 |
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
| `lobby_create_handlers` | apply create plan | plan 产出完整 lobby |
| `lobby_join_handlers` | apply join plan | 同上 |
| `lobby_lifecycle_handlers` | apply launch_init plan | 同上 |
| `lobby_launch_coordinator` | apply serversetup lobby | 自定义服 setup |
| `lobby_slot_handlers` | kick 后回滚 `before_lobby` | 失败/踢人边界 |
| `chat_handlers` | leave postgame 后 `{}` | 不 re-publish stale shared（见 shared-lobby-state-contract） |
| `steam_game_coordinator` | `GBE_ClearDotaLobbyRuntimeState` → `{}` | 全清 |
| `lobby_state_restore_coordinator` | adopt 失败/空 shared 时 `{}` | restore 路径 |
| `custom_game_lifecycle_coordinator` | 对侧 client restore 指针 | 跨 GC 恢复 |

规则：新增全量赋值须有 plan/restore/clear 语义；禁止在 payload helper 里整结构覆盖。

## 4. Local 字段就地写（摘要）

- **state / game_state / launch_***：queued-state 路径先经 `compose_queued_lobby_state_apply_plan()` 计算，再由 `apply_queued_lobby_state_apply_plan()` 一次写入三字段；monotonic phase 推进经 `advance_launch_phase()`；generic lobby capture 经 `compose_generic_lobby_state_capture_plan()` 与 `apply_generic_lobby_state_capture_plan()` 保留 launch 回退保护；generic capture 的 room/match/server/connect/start_time 经 `compose_generic_lobby_runtime_identity_capture_plan()` 与 `apply_generic_lobby_runtime_identity_capture_plan()`，保留 launched LAN runtime 保护；generic capture 的 allow_cheats/fill_with_bots/allow_spectating/visibility/bot_* 经 `compose_generic_lobby_options_capture_plan()` 与 `apply_generic_lobby_options_capture_plan()`；shared restore 的 runtime 字段组经 `compose_shared_lobby_runtime_restore_plan()` 与 `apply_shared_lobby_runtime_restore_plan()`，保留 READYUP state 回退保护。
- **members / chat / broadcast**：slot、chat、member_coordinator。
- **owner_hero_id**：只允许 `HOST_AUTHORITY` 列出的 Apply/adopt/publish 路径（禁止 match/inventory 旁路 `=`）。
- **server_id / connect / match_id**：recover、launch、restore merge；shared-to-local restore 的 connect/match_id 经 `restore_lobby_connect()` 与 `restore_lobby_match_id()` 返回变更。
- **game_start_time / room_name restore**：shared-to-local restore 经 `restore_lobby_game_start_time()` 与 `restore_lobby_room_name()` 返回变更；零 start_time 拒绝。
- **owner_connected / owner_team / owner_slot restore**：shared-to-local restore 经 `restore_lobby_owner_connected()`、`restore_lobby_owner_team()` 与 `restore_lobby_owner_slot()` 返回变更。
- **options restore**：shared-to-local restore 的 game_mode/server_region/lan/ping/allow_cheats/fill_with_bots/allow_spectating/pass_key/visibility/bot_* 经 `compose_shared_lobby_options_restore_plan()` 与 `apply_shared_lobby_options_restore_plan()` 返回字段组变更。
- **cache restore**：shared-to-local restore 的 has_cache_version/cache_version/has_cache_service_id/cache_service_id/cache_service_list/has_cache_sync_version/cache_sync_version 经 `compose_shared_lobby_cache_restore_plan()` 与 `apply_shared_lobby_cache_restore_plan()` 返回字段组变更。
- **custom_game restore**：shared-to-local restore 经 `restore_lobby_custom_game()` 返回变更；比较复用 `custom_game_details_equal()`。
- **generation / generic_lobby_id restore**：shared-to-local restore 经 `restore_lobby_generation()` 与 `restore_lobby_generic_lobby_id()` 返回变更；generation counter 同步仍由 Coordinator 负责。
- **launch_steam_auth_***：仅 steam-auth ack 路径通过 `compose_steam_auth_ack_launch_plan()` 与 `apply_steam_auth_ack_launch_plan()` 一次写入 CRC、message sequence 与 ack 标记，再 publish shared state。
- **launch_4511_seen**：仅匹配 lobby 的 4511 通知通过 `mark_launch_4511_seen()` 幂等标记；首次变更才 publish shared state。
- **launch_4511_seen restore**：shared-to-local restore 通过 `restore_launch_4511_seen()` 返回字段变更；Coordinator 继续聚合 restore 的 `changed` 结果。
- **lifecycle state / game_state**：仅 `LobbyStateApply` action 通过 `apply_lifecycle_lobby_state()` 一次写入；action 序列和后续 publish 继续由 lifecycle executor 管理。
- **postgame state / chat / cache**：仅 `PostGameLobbyStateApply` action 通过 `apply_postgame_lobby_state_plan()` 一次写入 state、chat 与 cache 清理字段组；executor 继续管理 action 序列与 publish。

## 5. C1 结论

1. Shared 写路径已单一化到 generation 门控门面；无需本轮改 Store API。
2. Local 仍是广泛工作副本；queued-state、monotonic launch phase、generic capture state/identity/options、shared runtime restore、steam-auth 元数据、4511 标记/restore、connect/match/start_time/room/owner/options/cache/custom_game/generation/generic_lobby_id restore、lifecycle/postgame state apply 已采用纯 apply 边界。后续字段组按单路径、单边界推进，减少 handler 内零散字段写。
3. 双轨（local + shared）风险仍在 CURRENT；本清单只冻结入口，不声明状态单一化完成。

## 6. 停手

- 不为美观搬 `GBE_local_lobby` 字段访问。
- 不改 host 权威字段语义（见 HOST_AUTHORITY）。
- 不在本轮引入 CompositionRoot 生产装配。
