# Host 状态权威表（代码锚点）

> 只描述当前代码读写纪律，不声明“已验收”。  
> 测试：`tools/gbe_dota_dual_gc_host_test/` 中 H1–H5 必须与本表一致。

## 1. 拓扑

| 角色 | 存储 | 说明 |
|------|------|------|
| client GC | `Steam_Game_Coordinator::GBE_local_lobby` | `is_server=false`，进程内实例 |
| server GC | `Steam_Game_Coordinator::GBE_local_lobby` | `is_server=true`，进程内另一实例 |
| shared | `gbe::dota_lobby_state::Store` → `GBE_SharedDotaLobbyState` | `steam_client.cpp` 双 GC 共用同一 Store |
| one-shot | server/client GC 成员键 | showcase / wearable，带 generation |

生产装配：`dll/steam_client.cpp` 约 151–183 行。

## 2. 字段权威

| 字段 | 权威 writer | 允许 reader | generation 条件 | 代码锚点 |
|------|-------------|-------------|------------------|----------|
| `owner_hero_id` (local) | `GBE_ApplyOwnerHeroId` / `apply_owner_hero_id`（lifecycle `LobbyMemberRuntimeUpdate` 经此） | 7034 响应、2569 推断后、equip replay | local.generation 与 lobby 绑定 | `gbe_dota_lobby_launch_coordinator.cpp`；`match_handlers`；`inventory_handlers` |
| `owner_hero_id` (shared) | `publish_local_lobby_to_shared`：非 `should_preserve_known_owner_hero_on_publish` 时写入 | 对侧 adopt / restore 增量 | shared.generation 来自 local.generation | `gbe_dota_lobby_state.cpp` |
| preserve on publish | `should_preserve_known_owner_hero_on_publish` | — | 同 lobby_id + owner_steam_id | 同上 |
| preserve on adopt | `should_preserve_known_owner_hero_on_adopt` | — | 同 lobby_id + owner_steam_id | 同上；restore 增量用 `apply_owner_hero_from_shared` |
| H1 peer restore | `should_peer_restore_owner_hero_from_client` + lifecycle apply client hero | server 后续 equip | lobby+generation+owner 一致 | `match_handlers` 7034 response 前 |
| equipped cache | `GBE_PushDotaPlayerEquippedItemsCacheToGC` / Mirror | 对侧 GC 队列 | 调用方 reason 串 | `gbe_dota_gc_payload_helpers.cpp`；`gc_internal.h` |
| showcase one-shot | Mark 写 generation+lobby+owner_steam+hero | `host_showcase_equip_key_matches` / HasPushed | **当前** generation 且 hero!=0 | `gbe_dota_lobby_state` + `steam_game_coordinator` Has/Mark |
| wearable one-shot | Mark 写 generation+steam_id+hero_id | `host_wearable_refresh_key_matches` / HasRefreshed | **当前** generation | 同上 |
| members[].hero_id | 7034 runtime 更新 / member helpers | 7034 响应、payload 构建 | 随 local lobby | `gbe_dota_match_handlers.cpp` |

## 3. 跨 GC 推装顺序（host 热路径）

1. 推断/恢复 `owner_hero_id`（2569 或 7034 peer restore）  
2. `GBE_RefreshDotaHostEquippedItemsCache(server, …)` 或 `Push…ToGC`  
3. 可选：`GBE_PushDotaHeroEquippedItemUpdatesToClientGC` + `MarkDotaHostLocalWearablesRefreshed`  
4. showcase：`!HasPushed…` 时 push 并 `MarkDotaHostShowcaseEquipPushed`

Clear 时机：reset / clear runtime / 2569 相关 clear（`inventory_handlers` 对 server Clear showcase/wearable）。

## 4. 测试映射

| 场景 | 表条款 |
|------|--------|
| H1 | peer restore from client local |
| H2 | showcase one-shot |
| H3 | wearable one-shot |
| H4 | generation 前进使 one-shot 失效 |
| H5 | publish/adopt preserve_known_owner_hero |
