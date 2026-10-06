# Agent 协作手册（GC 重构）

## 开工前

1. 读 [CURRENT.md](./CURRENT.md)
2. 认领 [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md) 中一条，标 in_progress
3. 若动路由 / host / 黄金路径：先打开对应表，确认行存在

## 改代码时

| 意图 | 必更文档 | 禁止 |
|------|----------|------|
| 新/改 emsg 入口 | MESSAGE_ROUTING_INVENTORY | 只加 if/template |
| hero/wearable/showcase | HOST_AUTHORITY | 直接写字段绕过 API |
| 行为顺序 / 生命周期 | GOLDEN_PATHS（点名 PathID） | 只靠 audit 绿宣称完成 |
| 共享状态写入 | concurrency-ownership + Store 门控 | 生产裸 publish |

## 提交前

```bash
bash tools/run_gc_verification.sh --full
```

- 更新 ACTIVE_QUEUE 状态与（若动）真相表行
- 提交信息写清：行为目标 + PathID/emsg（若有）
- 小边界改动验证通过后可按项目习惯直接提交闸口提交

## 并行边界（建议）

| 轨道 | 范围 | 独占 |
|------|------|------|
| A 路由 | dispatcher / post_login / template 表 | `MESSAGE_ROUTING` + 上述 cpp |
| B host | apply hero / equip ports / 7034 host | `HOST_AUTHORITY` 字段 |
| C 测试 | smoke / dual_gc / golden，不改生产语义 | 测试与 fixture |

禁止两人同时大改 `steam_game_coordinator.cpp` 入口与 inventory equip 热路径。

## 停手信号

- 需要新旁路 if 或新默认 template case → 停，先改 MESSAGE_ROUTING 设计
- 需要第二套 owner_hero 规则 → 停，先改 HOST_AUTHORITY
- 只能靠 `--skip-audit` 或删审计通过 → 停
- 范围滑向 DI/拆类且不在 ACTIVE_QUEUE → 停，另开队列项

## 验证层级预期（勿夸大）

| 命令/套件 | 证明什么 | 不证明什么 |
|-----------|----------|------------|
| `_audit_gc_refactor.py` | 结构契约 | 协议正确 |
| header compile / 纯 store·SM 测 | 编译与纯逻辑 | 全 GC 行为 |
| handler smoke | 桩环境下副作用顺序 | 真 SDK/网络 |
| dual_gc_host | 规则函数 H1–H5 | 完整链接 GC |
| gc_replay_test | payload summary | 不执行 handler |

## 记忆文件

项目行为规则摘要写在 `gbe_fork/.monkeycode/MEMORY.md`，细节以本目录真相表为准。
