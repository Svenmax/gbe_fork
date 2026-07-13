# 行为黄金路径（验收 KPI）

> 改 host / lobby / equip / launch / 7034 时，PR 必须点名影响的 PathID。
> 级别：L0 结构审计 · L1 纯逻辑 · L2 handler 行为（可 stub）· L3 dual-GC 规则 · L4 真协议闭环（尚缺）。

## 路径表

| PathID | 步骤 | 保护行为 | 测试位置 | 级别 | 状态 |
|--------|------|----------|----------|------|------|
| GP-01 | create →（invite）→ join | lobby 创建/加入、generation、缓存订阅顺序 | smoke: `test_lobby_create_*`；replay: `lobby_create_with_passkey` / `lobby_join_by_id` | L2 | 有 |
| GP-02 | equip(2569) → host showcase/wearable | hero 推断、推装顺序、one-shot | smoke: `test_inventory_equip_*`；dual_gc H2/H3/H4 | L2/L3 | 有（L3 不链全 GC） |
| GP-03 | 7034 runtime / host showcase | peer restore、runtime 先于响应、showcase 一次 | smoke: `test_match_7034_*`；dual_gc H1 | L2/L3 | 有 |
| GP-04 | leave / abandon / destroy / signout | teardown 顺序、shared 不被错误 clear | smoke: leave/abandon/destroy/signout；replay leave/kick | L2 | 有 |
| GP-05 | launch → state push | launch 副作用顺序、重复 push 跳过 | smoke: `test_lobby_launch_*`；replay `lobby_launch_allpick` | L2 | 有 |
| GP-06 | chat join/leave | 频道与 postgame 不 stale republish | smoke: `test_chat_*` | L2 | 有 |
| GP-07 | custom game 7070/8052/8053 | direct/wrapped 动作序列等价 | smoke: custom_game_lifecycle* | L2 | 有 |
| GP-08 | reconnect preserve vs clear | reset 原因决定是否清 reconnect | lobby_state / payload 决策单测；见历史 P0 | L1 | 部分 |
| GP-09 | Hello / ServerHello welcome | welcome 合成与 CacheSubscribed 时序 | L1：payload extract/build；L1.5：`plan_client_hello`（lobby_flow_test）；**L2 缺** | L1+L1.5 / L2 **缺** | 部分（E1 plan 已落地） |
| GP-10 | SendMessage 双实例往返 | client/server 真队列闭环 | **缺口** | L4 | **缺** |

## 改动 → 路径

| 触碰区域 | 至少覆盖 |
|----------|----------|
| registry / post_login / template | 相关 emsg 的 GP + MESSAGE_ROUTING 表 |
| owner_hero / equip / inventory ports | GP-02, GP-03 + HOST_AUTHORITY |
| lobby create/join/leave/launch | GP-01, GP-04, GP-05 |
| match 7034 | GP-03 |
| shared Store / restore / publish | GP-01, GP-04, GP-08 + dual_gc H5 |
| Hello / welcome_coordinator | GP-09（补测前谨慎改） |

## 验证命令

```bash
bash tools/run_gc_verification.sh --full
```

单测入口（按改动裁剪）：

- `tools/gbe_dota_handler_test/`（smoke + behavior_replay）
- `tools/gbe_dota_dual_gc_host_test/`
- `tools/gc_replay_test/`（payload summary，不执行 handler）

## 缺口优先级

1. GP-09 L2：ClientHello direct 最小 smoke 或 pure plan（**E1**，见 [PHASE_E_BOUNDARY.md](./PHASE_E_BOUNDARY.md)）
2. GP-09 L2：ServerHello skip/push 序（**E2**）
3. GP-02/03 在更少 stub 下的稳定性（**E3**）
4. GP-10 in-process 双 GC `SendMessage_` 闭环（**E4**，先设计）
