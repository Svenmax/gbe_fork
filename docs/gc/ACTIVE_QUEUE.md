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
| 2026-07-25 | parser generic section end：解析层 §5 截止于任意下一个二级标题，避免缺失分隔线时扩大 legacy parser 扫描 | `.monkeycode/specs/gc-parser-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | fallback generic section end：fallback §2 截止于任意下一个二级标题，避免缺失分隔线时扩大 direct/wrapped 扫描 | `.monkeycode/specs/gc-fallback-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template_replay generic section end：template_replay §4 截止于任意下一个二级标题，避免缺失分隔线时扩大扫描 | `.monkeycode/specs/gc-template-replay-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry generic section end：Registry inventory §1 截止于任意下一个二级标题，避免缺失分隔线时扩大扫描 | `.monkeycode/specs/gc-registry-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge generic section end：Local/shared merge inventory §5 截止于任意下一个二级标题，避免后续章节编号变化扩大扫描 | `.monkeycode/specs/gc-local-shared-merge-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge section presence regression：补齐 Local/shared merge inventory §5 缺失 fail-fast 的回归测试 | `.monkeycode/specs/gc-local-shared-merge-section-presence-regression/`、`tools/test_audit_gc_refactor.py` |
| 2026-07-25 | legacy wrapped parser section presence guard：缺失解析层 §5 时 legacy wrapped parser audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-legacy-wrapped-parser-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry section presence guard：缺失 registry §1 时 registry inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-registry-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | wrapped hard miss section presence guard：缺失 fallback §2 时 wrapped hard miss inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-wrapped-hard-miss-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | direct conditional section presence guard：缺失 fallback §2 时 direct conditional inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-direct-conditional-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry-defensive section presence guard：缺失 template_replay §4 时 registry-defensive inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-registry-defensive-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template-only section presence guard：缺失 template_replay §4 时 template-only inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-template-only-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template-only section guard：`TEMPLATE_ONLY` inventory 行只在 template_replay §4 中有效，避免其它段落行误参与 audit | `.monkeycode/specs/gc-template-only-section-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | legacy wrapped parser section guard：`GBE_ExtractWrappedDotaDirectContext` 的 `LEGACY_UNUSED` 标记只在解析层 §5 中有效，避免其它段落文字误满足 audit | `.monkeycode/specs/gc-legacy-wrapped-parser-section-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | wrapped hard miss section guard：wrapped HARD_MISS inventory 标记只在 fallback §2 中有效，避免其它段落文字误满足 audit | `.monkeycode/specs/gc-wrapped-hard-miss-section-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | direct conditional duplicate inventory guard：direct conditional fallback inventory 重复 emsg 行由 audit 检测，且解析限定在 fallback §2 | `.monkeycode/specs/gc-direct-conditional-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry-defensive duplicate inventory guard：registry-defensive inventory 重复 emsg 行由 audit 检测，且解析限定在 template_replay §4 | `.monkeycode/specs/gc-registry-defensive-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template-only duplicate inventory guard：template-only inventory 重复 emsg 由 audit 检测，避免 set 比较隐藏重复表格项 | `.monkeycode/specs/gc-template-only-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry duplicate inventory guard：registry inventory 重复 emsg 行由 audit 检测，避免 set/dict 比较隐藏重复表格行 | `.monkeycode/specs/gc-registry-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge duplicate inventory guard：Local/shared merge inventory 重复三元组由 audit 检测，避免 set 比较隐藏重复表格行 | `.monkeycode/specs/gc-local-shared-merge-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge line-comment guard：补齐 definition audit 的 line comment 伪定义回归测试，锁住 `//` 注释剥离语义 | `.monkeycode/specs/gc-local-shared-merge-line-comment-guard/`、`tools/test_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge comment definition guard：Local/shared merge definition audit 匹配前剥离 C++ 注释，避免 block comment 伪定义误通过 | `.monkeycode/specs/gc-local-shared-merge-comment-definition-guard/`、`tools/_audit_gc_refactor.py` |
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
