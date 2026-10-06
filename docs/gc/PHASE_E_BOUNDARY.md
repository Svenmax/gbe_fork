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
| **GP-09** Hello/Welcome | **L1** + **L1.5** plan；**L2 延期**（welcome 不可 offline 链，见 PHASE_E_EXIT） | 重启需可测 seam |
| **GP-10** 双 GC 往返 | **缺实现**；E4 设计有；**E5 S0 失败已延期** | 见 GP10 §13 |

## 3. 推荐切片顺序

| 优先级 | 切片 | 完成定义 | 停手 |
|--------|------|----------|------|
| E0 | 文档对齐 | GOLDEN_PATHS 反映 L1/L2 分档；本文件为入口 | 不写长交付文 |
| E1 | GP-09 ClientHello plan（L1.5） | **已完成：** `plan_client_hello` + 生产消费；lobby_flow_test | 不链 welcome_coordinator 进 smoke |
| E2 | GP-09 ServerHello plan（L1.5） | **已完成：** `plan_server_hello` skip/push/cache；生产消费；lobby_flow_test | 不合成 7034；不改 cache 字节 |
| E3 | GP-02/03 少 stub | 仅当 HOST/equip 回归痛时 | 不扩 dual_gc 到全 GC |
| E4 | GP-10 设计 | **已完成：** `GP10_L4_HARNESS.md`（档 A/B、泵契约、S0–S4、延期理由） | **无设计不写 L4 代码**；实现另开 E5 |
| E5 | GP-10 实现 | **延期：** S0 失败（GP10 §13 / PHASE_E_EXIT） | 无新链接策略不重启 |

**Phase E 主线：** 见 [PHASE_E_EXIT.md](./PHASE_E_EXIT.md)（2026-07-13 关闭）。

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
