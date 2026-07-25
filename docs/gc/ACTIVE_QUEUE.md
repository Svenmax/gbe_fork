# 活跃队列（WIP ≤5）

> 只保留进行中与本周明确不做。历史勾选见 archive 长文，不在此膨胀。
> **最后更新：** 2026-07-25

## WIP

| ID | 目标 | 触碰文件（预期） | 完成定义 | 验证 | 停手条件 |
|----|------|------------------|----------|------|------|
| （空） | R2 审计收尾、R3 聚合头与公开符号审计收尾、R4.1 至 R4.6、queued launch/runtime、monotonic launch phase、generic capture、shared runtime restore、steam-auth 元数据、4511 去重标记与 lifecycle state apply 单写入切片已完成；默认 D0 修 bug | — | — | verification --full | PHASE_D / PHASE_E_EXIT |

## 本周不做

- 真 DI / 拆 `Steam_Game_Coordinator` 上帝类（Phase D 大实施）
- CompositionRoot 进生产
- 为美观再横向大拆文件
- 重启 GP-10 L4 / GP-09 L2（无新链接策略）
- 新增长篇 delivery / next-agent 清单

## 最近完成（最多 5，旧项归档至 `REFACTOR_EXECUTION_PLAN.md`）

| 日期 | 摘要 | 链接 |
|------|------|------|
| 2026-07-25 | D2 切片：lifecycle `LobbyStateApply` 收敛为 state/game_state 原子 apply，保留 action 与 publish 顺序 | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：4511 launch 标记收敛为幂等 apply 边界，保留首次 publish 与重复去重 | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：steam-auth CRC、sequence 与 ack 标记收敛为纯 plan/apply，保留 publish 与 synthetic push 顺序 | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：shared restore 的 state/game_state/launch_phase 收敛为纯 plan/apply，保留 READYUP 回退保护 | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：generic lobby capture 的 state/game_state merge 收敛为纯 plan/apply，保留 launch 回退保护 | `REFACTOR_EXECUTION_PLAN.md` |

## 队列规则

1. 认领：把一行标 `in_progress`（或备注 Agent 名），同时 WIP 不超过 5。
2. 完成：写完成定义勾选结果 → 移入“最近完成” → 更新 CURRENT 风险若变化。
3. 冲突：HOST 字段相关任务与路由大迁移不要并行改同一热路径。
