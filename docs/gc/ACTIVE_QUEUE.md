# 活跃队列（WIP ≤5）

> 只保留进行中与本周明确不做。历史勾选见 archive 长文，不在此膨胀。
> **最后更新：** 2026-07-13

## WIP

| ID | 目标 | 触碰文件（预期） | 完成定义 | 验证 | 停手条件 |
|----|------|------------------|----------|------|------|
| C1 | local lobby 短期化 / Store 写路径审计 | lobby_state + CURRENT 风险 | 生产写 shared 仅 Store 门控；local 使用点清单 | verification --full | 不改 host 权威字段语义 |

## 本周不做

- 真 DI / 拆 `Steam_Game_Coordinator` 上帝类（Phase D）
- CompositionRoot 进生产
- 为美观再横向大拆文件
- 新增长篇 delivery / next-agent 清单

## 最近完成（最多 5，旧项删或链 archive）

| 日期 | 摘要 | 链接 |
|------|------|------|
| 2026-07-13 | B4：删 wrapped dead SetTeamSlot fallback；解析层分责文档 | post_login_handlers + MESSAGE_ROUTING §5 |
| 2026-07-13 | B3：template 白名单；8879/8095 改 REGISTRY_DEFENSIVE 转调 | template_replay + MESSAGE_ROUTING |
| 2026-07-13 | B2：Hello 迁 welcome_coordinator 显式 handler；入口薄路由 | steam_game_coordinator + welcome_coordinator |
| 2026-07-13 | B1：8009/7091 抽 handler 进 registry（50→52）；template 转调 | post_login + dispatcher + template |
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
