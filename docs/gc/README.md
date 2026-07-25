# Dota GC 文档

## 当前入口（先读）

| 文档 | 用途 |
|------|------|
| **[CURRENT.md](./CURRENT.md)** | **唯一状态入口**：硬规则、风险、验证、阅读顺序 |
| [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md) | 进行中任务（WIP≤5） |
| [MESSAGE_ROUTING_INVENTORY.md](./MESSAGE_ROUTING_INVENTORY.md) | 四轨路由真相表 |
| [HOST_AUTHORITY.md](./HOST_AUTHORITY.md) | host hero / wearable / showcase 契约 |
| [GOLDEN_PATHS.md](./GOLDEN_PATHS.md) | 行为黄金路径与测试映射 |
| [AGENT_PLAYBOOK.md](./AGENT_PLAYBOOK.md) | 多 Agent 协作与停手信号 |
| [REFACTOR_BASELINE.md](./REFACTOR_BASELINE.md) | 可复现重构基线与验证证据 |
| [STATE_OWNERSHIP.md](./STATE_OWNERSHIP.md) | 状态所有权、Store 契约与锁域 |
| [PROTOCOL_BEHAVIOR_MATRIX.md](./PROTOCOL_BEHAVIOR_MATRIX.md) | 生产协议入口、通道与 fixture 矩阵 |
| [REFACTOR_EXECUTION_PLAN.md](./REFACTOR_EXECUTION_PLAN.md) | 阶段化重构目标与验收条件 |

维护入口也可从 `.monkeycode/docs/INDEX.md` 进入架构/测试长文；**任务与路由权威以本目录 CURRENT + 真相表为准**。

## 验证

```bash
bash tools/run_gc_verification.sh --full
```

## 支撑契约（仍有效，非入口）

- `coordinator-boundaries.md` — coordinator / handler / state / payload 边界
- `shared-lobby-state-contract.md` — shared lobby 契约
- `concurrency-ownership.md` — 锁与 generation 域
- `lifecycle-state-map.md` — 生命周期路径
- `test-coverage-map.md` — offline 测覆盖
- `verification-and-build.md` — 构建与闸口
- `dependency-ownership-map.md` / `dependency-seams.md` — 依赖与副作用缝
- `response-seam-status.md` / `reason-trace-governance.md` — 响应与 reason 串
- `architecture-investment-gates.md` + `architecture-investment-inputs.json` — 投资门禁
- `internal-header-shrink-plan.md` — gc_internal 瘦身

## 历史档案（勿作权威入口）

下列文档保留设计/勾选历史；与 CURRENT 冲突时以 **CURRENT + 代码** 为准：

- `follow-up-task-list.md`、`next-agent-task-list.md`、`future-refactor-plan.md`
- `refactor-next-tasklist.md`、`launch-state-push-planner-tasklist.md`、`launch-state-side-effect-executor-tasklist.md`
- `shared-lobby-state-clear-plan.md`
- `.monkeycode/docs/GC_DELIVERY_SUMMARY.md`、阶段 specs tasklist

## 规则摘要

1. 新 Agent：CURRENT → 认领 ACTIVE_QUEUE → 改表再改码。
2. 新 emsg：只进 registry，并更新 MESSAGE_ROUTING。
3. host 字段：只经 HOST_AUTHORITY 允许的 API。
4. 文档不计入“重构完成”；完成看 Phase A–E 代码 KPI。
