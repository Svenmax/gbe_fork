# GC 重构当前真相（唯一入口）

> 任何 Agent 动手前只先读这一份。深入表见文末阅读顺序。
> 本文件描述代码现状，不声明“重构已完成 / 已验收”。

**最后同步：** 2026-07-13（对照 `dll/` 与 `tools/` 代码）

## 状态一句话

过渡态：Phase B/C 主路径已收口。Phase D/E **边界已书面化**；GP-09 **E1/E2 pure plan（L1.5）已落地**，handler L2 与 GP-10 L4 仍缺。默认不真 DI、不为测大拆。Store 门控 + local/shared 双轨仍在。

## 硬规则（违反即停手）

1. **新消息**只进 `GBE_ProductionDotaHandlerRegistry()`（`dll/gbe_dota_post_login_dispatcher.cpp`），不得只加 if-chain / template。
2. **生产写 shared lobby** 只走 Store generation 门控 API；禁止裸 `publish` / `update` / `clear`（审计 `audit_store_write_discipline`）。
3. **CompositionRoot** 仅 offline 测试；生产装配在 `dll/steam_client.cpp`，禁止生产构造 CompositionRoot。
4. **owner_hero_id / showcase / wearable** 只允许 `HOST_AUTHORITY.md` 列出的 writer；禁止在 match / inventory / restore 各写一套。
5. 改路由或 host 字段时，**先改对应真相表，再改代码**。

## 当前主风险（最多 5）

1. 路由仍多轨（registry / 条件 fallback / 白名单 template / Hello welcome），但每轨已有归属标签。
2. `GBE_local_lobby` 与 shared Store 双轨；restore/publish 字段级 merge 重（写入口清单见 `LOCAL_LOBBY_USAGE.md`）。
3. `Steam_Game_Coordinator` 仍是上帝类（成员 handler 面过大；Phase D）。
4. lifecycle 门闩是具名 gate token，载荷在 planner/action_list；不回退 `Legacy*` EffectKind。
5. 测试护栏强、真协议 L3/L4 弱；GP-09 有 L1+L1.5（plan），**L2 仍缺**；`verification passed` ≠ 协议正确。

## 进行中工作

见 [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md)（WIP ≤5）。默认下一刀：Phase E **E4**（GP-10 设计）或 D0 纪律下修 bug。

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
4. [LEGACY_LIFECYCLE_EFFECTS.md](./LEGACY_LIFECYCLE_EFFECTS.md) — lifecycle 具名门闩盘点
5. [PHASE_D_BOUNDARY.md](./PHASE_D_BOUNDARY.md) — Phase D 边界与停手
6. [PHASE_E_BOUNDARY.md](./PHASE_E_BOUNDARY.md) — Phase E 测补强边界
7. [GOLDEN_PATHS.md](./GOLDEN_PATHS.md) — 行为黄金路径
8. [AGENT_PLAYBOOK.md](./AGENT_PLAYBOOK.md) — 多 Agent 协作
9. [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md) — 当前任务

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
