# 活跃队列（WIP ≤5）

> 只保留进行中与本周明确不做。历史勾选见 archive 长文，不在此膨胀。
> **最后更新：** 2026-07-13

## WIP

| ID | 目标 | 触碰文件（预期） | 完成定义 | 验证 | 停手条件 |
|----|------|------------------|----------|------|------|
| （空） | R4.1 至 R4.6 已完成；默认 D0 修 bug | — | — | verification --full | PHASE_D / PHASE_E_EXIT |

## 本周不做

- 真 DI / 拆 `Steam_Game_Coordinator` 上帝类（Phase D 大实施）
- CompositionRoot 进生产
- 为美观再横向大拆文件
- 重启 GP-10 L4 / GP-09 L2（无新链接策略）
- 新增长篇 delivery / next-agent 清单

## 最近完成（最多 5，旧项删或链 archive）

| 日期 | 摘要 | 链接 |
|------|------|------|
| 2026-07-24 | R4.6：7004 followup details/25/pending 经 action list 与 executor | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-24 | R4.5：7035 deferred reset 经 action list 与 executor | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-24 | R4.4：7004 postgame state/publish/details 经 action list 与 executor | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-24 | R4.3：8246 immediate clear 经独立 decision 与 reset action executor | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-24 | R4.2：7041 launch-init reset/publish 经 action list 与 lifecycle executor | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-24 | R4.1：create/join generation boundary 经纯 lifecycle decision，保持 planner 与协议序 | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-13 | Phase E 出口：S0 失败 → GP-10/GP-09 L2 延期；主线关闭 | `PHASE_E_EXIT.md` |
| 2026-07-13 | E4：GP-10 L4 harness 书面设计 | `GP10_L4_HARNESS.md` |
| 2026-07-13 | E2：`plan_server_hello` + 生产 + flow 测 | welcome_flow |
| 2026-07-13 | E1：`plan_client_hello` + 生产 + flow 测 | welcome_flow |
| 2026-07-13 | Phase E 边界 + GP-09 分档（L1 有 / L2 缺） | `PHASE_E_BOUNDARY.md` + GOLDEN |
| 2026-07-13 | Phase D 边界书面化（准入/切片/停手；默认不实施） | `PHASE_D_BOUNDARY.md` |
| 2026-07-13 | C-exit：Legacy 路径归零复核；CURRENT 风险 #4 改为门闩形态说明 | LEGACY + CURRENT + ACTIVE |
| 2026-07-13 | C9：QueuePostGame 状态突变 action 化（PostGameLobbyStateApply + publish/RP） | launch_coordinator + flow + executor |
| 2026-07-13 | C8：member runtime 统一 decide_member_runtime_actions（match/inventory/network） | lifecycle_actions + handlers |
| 2026-07-13 | C7：custom_game decide_* 单一入口（SM+compute 出 handler） | custom_game_lifecycle_* |
| 2026-07-13 | C6：teardown 路径门闩 + preflight/list leave action_list；删 TeardownActionsRequested | SM + flow + lifecycle/list |
| 2026-07-13 | A3：host hero 审计；adopt 非 0 走 apply_owner_hero_id | lobby_state + HOST_AUTHORITY |
| 2026-07-13 | A2：if-chain 主体迁 registry（37→50），direct 仅留条件/8744/template | dispatcher + post_login_handlers |
| 2026-07-13 | 文档入口：CURRENT + 五份真相表 + MEMORY 指向 | `docs/gc/CURRENT.md` 等 |
| 2026-07-13 | D.12 template 资产分离 + gc_internal slim | 提交 `a0b88593` 一带 |
| 2026-07-13 | launch 链 / 高风险 post-login 迁 registry | `d7a78f98` / `ec4cdc56` |
| 2026-07-13 | Store generation 写纪律 | `293f2af0` |

## 队列规则

1. 认领：把一行标 `in_progress`（或备注 Agent 名），同时 WIP 不超过 5。
2. 完成：写完成定义勾选结果 → 移入“最近完成” → 更新 CURRENT 风险若变化。
3. 冲突：HOST 字段相关任务与路由大迁移不要并行改同一热路径。
