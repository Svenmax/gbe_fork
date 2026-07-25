# GC 重构当前真相（唯一入口）

> 任何 Agent 动手前只先读这一份。深入表见文末阅读顺序。
> 本文件描述代码现状，不声明“重构已完成 / 已验收”。

**最后同步：** 2026-07-25（对照 `dll/`、`tools/` 与 `docs/gc/ACTIVE_QUEUE.md`）

## 状态一句话

**收敛出口（2026-07-25）：** 本轮双轨收敛规划任务已完成；R2 Dependencies、R3 capability-header 审计收尾、R4.1–R4.6 局部生命周期单向化、queued launch/runtime、monotonic launch phase、generic capture state/identity/options/custom_game 与聚合 apply、payload 纯 snapshot projection、显式 host capture 同步边界、source-aware shared launch/runtime identity restore、steam-auth 元数据、4511 标记/restore、owner/options/cache/custom_game/generation/generic_lobby_id restore、lifecycle/postgame state apply 单写入切片已完成。generic metadata publish 已实施：7009 host sync、client observe 与 cache/replay/details pure projection 调用面由审计契约保护；8052 lifecycle pre-write 已实施：direct/wrapped 统一经 `LocalLifecyclePreWrite` action 在 runtime queue / fallback publish 前一次写入 Local lifecycle 字段组；postgame chat tombstone 已实施：old-channel 7272 与 7014 retrieval 经显式 tombstone channel/generation helper 消费；Local/shared merge inventory guard 已实施：shared-to-local restore 字段组 entrypoints、owner 与字段组标签由 `LOCAL_LOBBY_USAGE.md` 和剥离注释后的函数定义级 audit 保护，重复清单行、block comment 与 line comment 伪定义由回归覆盖；registry inventory guard 已实施：production registry kTable 与 routing inventory §1 的 emsg 集合、HandlerId、modes、lifecycle 与重复 emsg 行由审计保持同步；template-only inventory guard 已实施：template replay 的 `TEMPLATE_ONLY` switch case、routing inventory 白名单与重复 emsg 由审计保持同步；template registry-defensive routing 已实施：`8879`、`8095`、`8009`、`7091` 经显式 helper 防御转调，§4 routing inventory 集合与重复 emsg 行由审计保护；direct conditional fallback routing 已实施：`8744` observe-only 与 `5410`/`5432` late-steam conditional consume 经显式 helper，§2 routing inventory 集合与重复 emsg 行由审计保护；wrapped hard miss routing 已实施：wrapped registry miss 经显式 helper 保持 log + return false 并由路由审计保护；legacy wrapped parser guard 已实施：`GBE_ExtractWrappedDotaDirectContext` 保持 `LEGACY_UNUSED`，生产 `.cpp` 无调用 contract 由审计保护。Phase D **边界+默认不实施**；Phase E **主线关闭**（plan L1.5 + GP-10 设计；L2/L4 链接 spike 失败已书面延期，见 `PHASE_E_EXIT.md`）。日常 D0 修 bug。Store 门控在；local/shared 双轨与上帝类仍为已知债。

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

见 [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md)。`gc-direct-conditional-duplicate-inventory-guard`、`gc-registry-defensive-duplicate-inventory-guard`、`gc-template-only-duplicate-inventory-guard`、`gc-registry-duplicate-inventory-guard`、`gc-local-shared-merge-duplicate-inventory-guard`、`gc-local-shared-merge-line-comment-guard`、`gc-local-shared-merge-comment-definition-guard`、`gc-local-shared-merge-definition-guard`、`gc-local-shared-merge-fieldgroup-guard`、`gc-local-shared-merge-inventory-guard`、`gc-generic-metadata-publish`、`gc-8052-lifecycle-pre-write`、`gc-postgame-chat-tombstone`、`gc-registry-inventory-guard`、`gc-registry-metadata-inventory-guard`、`gc-template-only-inventory-guard`、`gc-template-registry-defensive-routing`、`gc-direct-conditional-fallback-routing`、`gc-wrapped-hard-miss-routing` 与 `gc-legacy-wrapped-parser-guard` 已完成。R2、R3、R4.1–R4.6、queued launch/runtime、monotonic launch phase、generic capture state/identity/options/custom_game 与聚合 apply、payload 纯 snapshot projection、显式 host capture 同步边界、source-aware shared launch/runtime identity restore、steam-auth 元数据、4511 标记/restore、owner/options/cache/custom_game/generation/generic_lobby_id restore、lifecycle/postgame state apply 单写入切片已完成。重启 L2/L4 须满足 PHASE_E_EXIT / GP10 §13。

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
