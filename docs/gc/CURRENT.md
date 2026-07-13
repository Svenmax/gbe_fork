# GC 重构当前真相（唯一入口）

> 任何 Agent 动手前只先读这一份。深入表见文末阅读顺序。
> 本文件描述代码现状，不声明“重构已完成 / 已验收”。

**最后同步：** 2026-07-13（对照 `dll/` 与 `tools/` 代码）

## 状态一句话

过渡态：Phase B 路由收敛已落地。Phase C：Store 写已门控 + local 写清单；lifecycle SM 门闩均已具名，teardown/custom_game/member runtime/QueuePostGame 已 decide 或 action_list 化（见 `LEGACY_LIFECYCLE_EFFECTS.md`）。

## 硬规则（违反即停手）

1. **新消息**只进 `GBE_ProductionDotaHandlerRegistry()`（`dll/gbe_dota_post_login_dispatcher.cpp`），不得只加 if-chain / template。
2. **生产写 shared lobby** 只走 Store generation 门控 API；禁止裸 `publish` / `update` / `clear`（审计 `audit_store_write_discipline`）。
3. **CompositionRoot** 仅 offline 测试；生产装配在 `dll/steam_client.cpp`，禁止生产构造 CompositionRoot。
4. **owner_hero_id / showcase / wearable** 只允许 `HOST_AUTHORITY.md` 列出的 writer；禁止在 match / inventory / restore 各写一套。
5. 改路由或 host 字段时，**先改对应真相表，再改代码**。

## 当前主风险（最多 5）

1. 路由仍多轨（registry / 条件 fallback / 白名单 template / Hello welcome），但每轨已有归属标签。
2. `GBE_local_lobby` 与 shared Store 双轨；restore/publish 字段级 merge 重（写入口清单见 `LOCAL_LOBBY_USAGE.md`）。
3. `Steam_Game_Coordinator` 仍是上帝类（成员 handler 面过大）。
4. lifecycle SM 半接入收口中：teardown/custom_game/member runtime/QueuePostGame 已 action 化；Phase C 出口仍待全量 Legacy 归零复核。
5. 测试护栏强、真协议 L3/L4 弱；`verification passed` ≠ 协议正确。

## 进行中工作

见 [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md)（WIP ≤5）。默认下一刀：Phase C 收口复核 / 评估 Phase D。

## 必跑验证

```bash
bash tools/run_gc_verification.sh --full
```

改 host / equip / 7034 时额外：

```bash
# dual_gc H1–H5 与 HOST_AUTHORITY 一致
# handler smoke 中与 GOLDEN_PATHS 相关的用例
```

## 深入阅读顺序

1. [MESSAGE_ROUTING_INVENTORY.md](./MESSAGE_ROUTING_INVENTORY.md) — 四轨入口表
2. [HOST_AUTHORITY.md](./HOST_AUTHORITY.md) — hero / wearable / showcase
3. [LOCAL_LOBBY_USAGE.md](./LOCAL_LOBBY_USAGE.md) — local/shared 写入口清单
4. [LEGACY_LIFECYCLE_EFFECTS.md](./LEGACY_LIFECYCLE_EFFECTS.md) — SM Legacy effect 与迁移序
5. [GOLDEN_PATHS.md](./GOLDEN_PATHS.md) — 行为黄金路径
6. [AGENT_PLAYBOOK.md](./AGENT_PLAYBOOK.md) — 多 Agent 协作
7. [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md) — 当前任务

## 历史文档（勿作权威入口）

以下保留作设计/勾选档案，**不以其中“已完成”表述覆盖 CURRENT**：

- `follow-up-task-list.md`、`next-agent-task-list.md`、`future-refactor-plan.md`
- `refactor-next-tasklist.md`、`launch-state-*-tasklist.md`
- `.monkeycode/docs/GC_DELIVERY_SUMMARY.md` 等交付长文
- specs 下阶段 tasklist（可作细节，冲突时以本目录真相表 + 代码为准）

## 收敛阶段（代码 KPI，文档不算完成）

| Phase | 目标 |
|-------|------|
| A | 冻结路由/host 规则 + 黄金路径清单落地 |
| B | 单路径路由（if-chain→0，Hello 迁出，template 白名单） |
| C | 状态单一化（local 短期化，Legacy effect 收敛） |
| D | 真 DI / handler 脱离上帝类 |
| E | L2/L3 行为验收升级 |

出口条件见审查结论：路由单一 + host 唯一写路径 + Store 门控 + 关键路径行为测绿。
