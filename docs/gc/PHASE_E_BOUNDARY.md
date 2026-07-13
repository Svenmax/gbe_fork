# Phase E 边界（行为/协议测补强）

> 对照代码日期：2026-07-13。**默认补测，不改运行时语义，不为测而大拆架构。**
> 主表：`GOLDEN_PATHS.md`、`test-coverage-map.md`、`MESSAGE_ROUTING_INVENTORY.md`。
> 与 Phase D 解耦：D 管宿主/依赖边界；E 管验收级别与缺口关闭。

## 1. 级别定义（与 GOLDEN_PATHS 一致）

| 级别 | 含义 | 代表 |
|------|------|------|
| L0 | 结构审计 | `_audit_gc_refactor.py` |
| L1 | 纯逻辑 / payload | lobby_flow/state、payload_helpers |
| L2 | handler 行为（可 stub） | `gbe_dota_handler_test` smoke |
| L3 | dual-GC 规则 | `gbe_dota_dual_gc_host_test` |
| L4 | 真协议闭环 | 双实例 `SendMessage_` 往返（**尚缺**） |

**Phase E 出口（渐进）：** GP 表无「关键路径仅有 L0」；GP-09 至少 L2；GP-10 有可重复 L4 或明确延期理由。

## 2. 缺口现状（评估）

| PathID | 现状 | 下一刀成本 |
|--------|------|------------|
| GP-01…07 | L2 为主，闸口内 | 维持；改路径时点名 PathID |
| GP-08 | L1 决策单测 + 部分 smoke | 新 reset reason 时扩展 L1 |
| **GP-09** Hello/Welcome | **L1 有**（extract/build/compose in payload_helpers_test）；**L2 无**（handler_test 未链 `welcome_coordinator`） | 中高：要 stub restore/login-sync/queue 或抽 pure plan |
| **GP-10** 双 GC 往返 | **缺** | 高：进程内双 GC + 真队列 |

## 3. 推荐切片顺序

| 优先级 | 切片 | 完成定义 | 停手 |
|--------|------|----------|------|
| E0 | 文档对齐 | GOLDEN_PATHS 反映 L1/L2 分档；本文件为入口 | 不写长交付文 |
| E1 | GP-09 L2 最小 | ClientHello **direct** 路径：parse 失败 → false；成功 → welcome push 序 +（可选）top_custom 标记；不要求 LoginSync 全真实现 | 不链全 `steam_game_coordinator`；不改 welcome 语义 |
| E2 | GP-09 ServerHello L2 | 无 active lobby：ServerWelcome push；已 welcome / 队列已有 welcome：skip 可断言 | 不合成 7034；不改 cache 字节 |
| E3 | GP-02/03 少 stub | 仅当 HOST/equip 回归痛时 | 不扩 dual_gc 到全 GC |
| E4 | GP-10 设计 | 书面 harness 边界（client/server GC 构造、消息泵） | **无设计不写 L4 代码** |

**默认本周：** 完成 E0；E1 仅在准入满足时动手。

## 4. E1 准入（ClientHello L2）

全部满足才改测试代码：

1. 明确只测 `GBE_HandleDotaClientHelloRequest` 的 **direct** 分支（或 pure plan 等价序）。
2. 依赖可 stub：`settings` steam/app id、`push_incoming_now` recorder、`GBE_PushDotaResponse` / `GBE_PushDotaLoginSyncMessages` 可空实现。
3. 不把 `welcome_coordinator.cpp` 整文件硬链进 smoke，除非 source list + 缺失符号已盘点且无生产语义改动。
4. 先更新 `GOLDEN_PATHS` GP-09 行，再合测试。
5. `verification --full` 绿。

**备选低成本路径：** 若成员函数链接面过大，先抽 **pure** `plan_client_hello_actions(...)`（仅 action 序，无 IO），L1.5 覆盖后再 L2。

## 5. 硬停手

- 为过测改 Hello/Welcome 生产消息序或 payload 字节
- 一次 PR 混做 E1 + 生产 welcome 重构 + Phase D 拆类
- 宣称 L4 完成但仅有 mock 无双队列
- CompositionRoot 进生产「方便测」

## 6. 验证

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

## 7. 维护

- 关闭任一缺口：更新 `GOLDEN_PATHS` 状态列 + 本表 §2。
- 与 `PHASE_D_BOUNDARY` 冲突时：测补强走 E；宿主拆分走 D。
