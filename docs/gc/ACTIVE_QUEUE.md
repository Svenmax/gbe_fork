# 活跃队列（WIP ≤5）

> 只保留进行中与本周明确不做。历史勾选见 archive 长文，不在此膨胀。
> **最后更新：** 2026-07-13

## WIP

| ID | 目标 | 触碰文件（预期） | 完成定义 | 验证 | 停手条件 |
|----|------|------------------|----------|------|------|
| E4 | GP-10 L4 harness 书面设计 | PHASE_E / GOLDEN 或独立短文 | client/server 构造、消息泵、完成定义、延期理由 | 文档 review | **无设计不写 L4 代码** |

## 本周不做

- 真 DI / 拆 `Steam_Game_Coordinator` 上帝类（Phase D 大实施）
- CompositionRoot 进生产
- 为美观再横向大拆文件
- GP-10 L4 无设计直接编码
- 新增长篇 delivery / next-agent 清单

## 最近完成（最多 5，旧项删或链 archive）

| 日期 | 摘要 | 链接 |
|------|------|------|
| 2026-07-13 | E2：`plan_server_hello` skip/push/cache + 生产消费 + flow 测 | welcome_flow + welcome_coordinator |
| 2026-07-13 | E1：`plan_client_hello` + 生产消费 + flow 测（GP-09 L1.5） | welcome_flow + welcome_coordinator |
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
