# 活跃队列（WIP ≤5）

> 只保留进行中与本周明确不做。历史勾选见 archive 长文，不在此膨胀。
> **最后更新：** 2026-07-25

## WIP

| ID | 目标 | 触碰文件（预期） | 完成定义 | 验证 | 停手条件 |
|----|------|------------------|----------|------|------|
| — | — | — | — | — | — |

## 本周不做

- 真 DI / 拆 `Steam_Game_Coordinator` 上帝类（Phase D 大实施）
- CompositionRoot 进生产
- 为美观再横向大拆文件
- 重启 GP-10 L4 / GP-09 L2（无新链接策略）
- 新增长篇 delivery / next-agent 清单

## 最近完成（最多 5，旧项归档至 `REFACTOR_EXECUTION_PLAN.md`）

| 日期 | 摘要 | 链接 |
|------|------|------|
| 2026-07-25 | Local/shared merge definition guard：Local/shared merge entrypoint presence 校验收紧为函数定义匹配，避免调用点或文本引用误判 | `.monkeycode/specs/gc-local-shared-merge-definition-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge field-group guard：Local/shared merge inventory 的 entrypoint、owner、字段组三元组由 audit 保持同步 | `.monkeycode/specs/gc-local-shared-merge-fieldgroup-guard/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | Local/shared merge inventory guard：shared-to-local restore 字段组 entrypoints 由 `LOCAL_LOBBY_USAGE.md` 与 audit 保持同步 | `.monkeycode/specs/gc-local-shared-merge-inventory-guard/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | registry metadata inventory guard：production registry kTable 与 routing inventory §1 的 emsg 集合、HandlerId、modes、lifecycle 由 audit 保持同步 | `.monkeycode/specs/gc-registry-metadata-inventory-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | registry inventory guard：production registry kTable 与 routing inventory §1 的 emsg 集合由 audit 保持同步 | `.monkeycode/specs/gc-registry-inventory-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | template-only inventory guard：template replay 的 `TEMPLATE_ONLY` switch case 与 routing inventory 白名单由 audit 保持同步 | `.monkeycode/specs/gc-template-only-inventory-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | legacy wrapped parser guard：`GBE_ExtractWrappedDotaDirectContext` 保持 `LEGACY_UNUSED`，生产 `.cpp` 无调用 contract 由审计保护 | `.monkeycode/specs/gc-legacy-wrapped-parser-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | wrapped hard miss routing：wrapped post-login registry miss 收敛到显式 hard-miss helper，保持 log + return false，并由 audit 保护 template replay / SetTeamSlot dead fallback 缺席 | `.monkeycode/specs/gc-wrapped-hard-miss-routing/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | direct conditional fallback routing：`8744` observe-only 与 `5410`/`5432` late-steam conditional consume 收敛到显式 helper，并由 audit 保护实现与路由真相表一致性 | `.monkeycode/specs/gc-direct-conditional-fallback-routing/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | template registry-defensive routing：`8879`、`8095`、`8009`、`7091` 从 template-only switch 收敛到显式 defensive helper，并由 audit 保护路由真相表一致性 | `.monkeycode/specs/gc-template-registry-defensive-routing/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | postgame chat tombstone：旧 postgame chat channel 显式记录 tombstone channel/generation，并以 helper 统一 old-channel 7272 与 7014 retrieval 的 generation-scoped 消费 | `.monkeycode/specs/gc-postgame-chat-tombstone/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | 8052 lifecycle pre-write：direct/wrapped 统一经 `LocalLifecyclePreWrite` action 在 runtime queue / fallback publish 前一次写入 Local lifecycle 字段组 | `.monkeycode/specs/gc-8052-lifecycle-pre-write/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | generic metadata publish：盘点全部 capture 调用点，冻结 7009 host sync、client observe、cache/replay/details pure projection 契约并增加审计回归 | `.monkeycode/specs/gc-generic-metadata-publish/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | D2 规划收尾：generic metadata publish、8052 lifecycle pre-write 与 postgame chat tombstone 已拆为独立待实施规格 | `.monkeycode/specs/gc-generic-metadata-publish/`、`.monkeycode/specs/gc-8052-lifecycle-pre-write/`、`.monkeycode/specs/gc-postgame-chat-tombstone/` |
| 2026-07-25 | D2 切片：same-generation Local generic capture 与 shared snapshot 竞争时，以 source-aware restore plan 保留 Local launch/runtime identity 字段组 | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：7009 host capture 经显式同步边界发布 shared Store，client 保持 observe-only | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：cache、replay 与 details payload 统一 capture facade | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：payload facade 收敛为纯 snapshot projection | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：generic metadata capture 收敛为聚合 plan/apply | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：generic capture 的 custom_game 字段组收敛为 plan/apply | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：generic capture 的 options/bot 字段组收敛为 plan/apply | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：generic capture 的 room/match/server/connect/start_time 收敛为 plan/apply | `REFACTOR_EXECUTION_PLAN.md` |

## 队列规则

1. 认领：把一行标 `in_progress`（或备注 Agent 名），同时 WIP 不超过 5。
2. 完成：写完成定义勾选结果 → 移入“最近完成” → 更新 CURRENT 风险若变化。
3. 冲突：HOST 字段相关任务与路由大迁移不要并行改同一热路径。
