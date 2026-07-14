# GC 重构当前真相（唯一入口）

> 任何 Agent 动手前只先读这一份。深入表见文末阅读顺序。
> 本文件描述代码现状，不声明“重构已完成 / 已验收”。

**最后同步：** 2026-07-13（对照 `dll/` 与 `tools/` 代码）

## 状态一句话

**收敛出口（2026-07-13）：** Phase A–C 主路径已收口；Phase D **边界+默认不实施**；Phase E **主线关闭**（plan L1.5 + GP-10 设计；L2/L4 链接 spike 失败已书面延期，见 `PHASE_E_EXIT.md`）。日常 D0 修 bug。Store 门控在；local/shared 双轨与上帝类仍为已知债。

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
5. 测试护栏强、真协议 L4/GP-09 L2 **已延期**；`verification passed` ≠ 协议正确。

## 进行中工作

见 [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md)。**重构收敛 WIP 为空**；默认 D0 修 bug。重启 L2/L4 须满足 PHASE_E_EXIT / GP10 §13。

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
6. [PHASE_E_BOUNDARY.md](./PHASE_E_BOUNDARY.md) / [PHASE_E_EXIT.md](./PHASE_E_EXIT.md) — Phase E 边界与出口
7. [GP10_L4_HARNESS.md](./GP10_L4_HARNESS.md) — GP-10 L4 设计 + S0 延期
8. [GOLDEN_PATHS.md](./GOLDEN_PATHS.md) — 行为黄金路径
9. [AGENT_PLAYBOOK.md](./AGENT_PLAYBOOK.md) — 多 Agent 协作
10. [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md) — 当前任务

## 历史文档（勿作权威入口）

以下保留作设计/勾选档案，**不以其中“已完成”表述覆盖 CURRENT**：

- `follow-up-task-list.md`、`next-agent-task-list.md`、`future-refactor-plan.md`
- `refactor-next-tasklist.md`、`launch-state-*-tasklist.md`
- `.monkeycode/docs/GC_DELIVERY_SUMMARY.md` 等交付长文
- specs 下阶段 tasklist（可作细节，冲突时以本目录真相表 + 代码为准）

## 收敛阶段（代码 KPI，文档不算完成）

| Phase | 目标 | 收敛态 |
|-------|------|--------|
| A | 冻结路由/host 规则 + 黄金路径 | **收口** |
| B | 单路径路由（registry 主轨） | **收口** |
| C | Legacy effect 收敛 / lifecycle decide | **收口** |
| D | 真 DI / 脱离上帝类 | **边界文档；默认不实施** |
| E | L2/L3/L4 测补强 | **主线关闭**（L1.5+设计+延期） |

**本轮收敛完成定义（已满足）：** A–C 代码 KPI 收口 + D/E 边界与停手书面化 + 关键路径至少 L1/L2 护栏（GP-01…07 L2；GP-09 L1.5；GP-10 延期）+ `verification --full` 可重复绿。

**明确不在本轮完成：** Phase D 大实施；GP-09 L2 / GP-10 L4 真协议；消灭 local/shared 双轨。
