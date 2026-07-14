# Host 状态权威表（正式契约）

> 只描述当前代码读写纪律，不声明“已验收”。
> 测试：`tools/gbe_dota_dual_gc_host_test/` 中 H1–H5 必须与本表一致。
> 同源 specs：`.monkeycode/specs/2026-07-12-gc-behavior-gate-stabilization/host_authority.md`（冲突以本文件 + 代码为准）。

## 1. 拓扑

| 角色 | 存储 | 说明 |
|------|------|------|
| client GC | `Steam_Game_Coordinator::GBE_local_lobby` | `is_server=false` |
| server GC | `Steam_Game_Coordinator::GBE_local_lobby` | `is_server=true` |
| shared | `gbe::dota_lobby_state::Store` → `GBE_SharedDotaLobbyState` | 双 GC 共用 |
| one-shot | GC 成员键 | showcase / wearable，带 generation |

生产装配：`dll/steam_client.cpp`（Locator 绑定 + registry + executor + 双 GC 构造）。

## 2. 字段权威

| 字段 | 权威 writer | 允许 reader | generation 条件 | 代码锚点 |
|------|-------------|-------------|------------------|----------|
| `owner_hero_id` (local) | `GBE_ApplyOwnerHeroId` / `apply_owner_hero_id`（lifecycle `LobbyMemberRuntimeUpdate` 经此） | 7034 响应、2569 推断后、equip replay | local.generation 与 lobby 绑定 | launch_coordinator；match_handlers；inventory_handlers |
| `owner_hero_id` (shared) | `publish_local_lobby_to_shared`：非 preserve 时写入 | 对侧 adopt / restore | shared.generation 来自 local | `gbe_dota_lobby_state.cpp` |
| preserve on publish | `should_preserve_known_owner_hero_on_publish` | — | 同 lobby_id + owner_steam_id | 同上 |
| preserve on adopt | `should_preserve_known_owner_hero_on_adopt` | — | 同 lobby_id + owner_steam_id | restore 用 `apply_owner_hero_from_shared` |
| H1 peer restore | `should_peer_restore_owner_hero_from_client` + lifecycle apply | server 后续 equip | lobby+generation+owner 一致 | match_handlers 7034 前 |
| equipped cache | `GBE_PushDotaPlayerEquippedItemsCacheToGC` / Mirror / Refresh ports | 对侧 GC 队列 | 调用方 reason | `gbe_dota_inventory_ports.h`；payload helpers |
| showcase one-shot | Mark：generation+lobby+owner_steam+hero | HasPushed / key_matches | **当前** generation 且 hero≠0 | lobby_state + GC Has/Mark |
| wearable one-shot | Mark：generation+steam_id+hero_id | HasRefreshed / key_matches | **当前** generation | 同上 |
| members[].hero_id | 7034 runtime / member helpers | 7034 响应、payload | 随 local lobby | match_handlers |

## 3. 唯一允许的写入 API（禁止旁路）

| 意图 | 只允许 |
|------|--------|
| 设置 local owner_hero（运行时） | `GBE_ApplyOwnerHeroId` / `apply_owner_hero_id`（**拒绝 hero==0**） |
| 从 shared 增量 hero | `apply_owner_hero_from_shared`（经 restore 路径） |
| 全量 adopt 覆盖/清空 hero | 仅 `adopt_shared_lobby_to_local`：非 0 走 `apply_owner_hero_id`，shared==0 才允许清 0 |
| publish 到 shared DTO | `publish_local_lobby_to_shared` 在 `!preserve` 时 `shared.owner_hero_id = local.owner_hero_id` |
| 推装备缓存到对侧 GC | `GBE_PushDotaPlayerEquippedItemsCacheToGC` / `GBE_RefreshDotaHostEquippedItemsCache` |
| 客户端 wearable 更新 | `GBE_PushDotaHeroEquippedItemUpdatesToClientGC` |
| showcase / wearable 一次性标记 | 现有 Mark* API（禁止手写成员键绕过） |
| 发布到 shared Store | generation 门控 publish / compare_*（禁止裸 publish） |

**禁止：** 在 match / inventory / payload 中直接 `GBE_local_lobby.owner_hero_id = …`。
**审计（2026-07-13）：** match 7034 / inventory 2569 经 lifecycle → `GBE_ApplyOwnerHeroId`；无旁路赋值。

## 4. 跨 GC 推装顺序（host 热路径）

1. 推断/恢复 `owner_hero_id`（2569 或 7034 peer restore）
2. `GBE_RefreshDotaHostEquippedItemsCache(server, …)` 或 `Push…ToGC`（server unsub+equipped CacheSub）
3. 可选 host-local wearable one-shot：
   - 先 `GBE_PushDotaPlayerEquippedItemsCacheToGC(client, unsub_first=true)`（与 server 同语义的 equipped replace）
   - 再 `GBE_PushDotaHeroEquippedItemUpdatesToClientGC` + **server+client** `callback_respawn_request`(0.1/1.5) + `MarkDotaHostLocalWearablesRefreshed`
   - 1029 必须双 GC：server 给对端/实体重建；client 给房主进程本地自见
   - 仅 hero_replay SOUpdate 不够：client login 全量 SO 仍保留旧装；empty-equip 增量无法保证清槽
4. PRE_GAME（request `game_state==4`）showcase one-shot：`!HasPushed…` 时先 `ClearDotaHostLocalWearablesRefreshed`，再 `OwnerHeroKnownEquipReplay` 并 `MarkDotaHostShowcaseEquipPushed`
   - 策略期 2569 可能已 mark wearable one-shot；PRE_GAME 必须清 key，否则房主本地 cache replace / emsg 26 / 1029 被跳过（对端仍可见）
   - 触发条件必须是 `==4`（PRE_GAME），不能用 `>=4`：`TEAM_SHOWCASE=8` 会先于 `PRE_GAME=4` 到达并耗尽 generation-bound one-shot

Clear：reset / clear runtime / 2569 相关 clear（inventory 对 server Clear showcase/wearable）。

## 5. 测试映射

| 场景 | 条款 |
|------|------|
| H1 | peer restore from client local |
| H2 | showcase one-shot |
| H3 | wearable one-shot |
| H4 | generation 前进使 one-shot 失效 |
| H5 | publish/adopt preserve_known_owner_hero |

改本表任一 writer → 跑 dual_gc 全套 + 相关 GOLDEN_PATHS（GP-02 / GP-03）。
