# Phase E 出口复核（2026-07-13）

> 对照代码 + S0 链接 spike。**本文件关闭 Phase E 文档/测补强主线**；不宣称真协议 L4 或上帝类拆分已完成。

## 1. 出口条件（原 PHASE_E）

| 条件 | 结果 |
|------|------|
| GP 表无「关键路径仅有 L0」 | **满足**（GP-01…08 至少 L1；GP-09 L1+L1.5） |
| GP-09 至少 L2 | **未满足代码 L2**；**书面延期**（§3） |
| GP-10 有可重复 L4 或明确延期理由 | **延期理由成立**（§2） |

## 2. GP-10 S0 链接 spike（证据）

**尝试：** 用 `gbe_dota_handler_test` 同款 stub 包装 `#include "dll/gbe_dota_welcome_coordinator.cpp"` 编译。

**结果：失败。** 缺/冲突面包括：

- stub 无 `GBE_HandleDotaServerHelloRequest` / `ClientHello` / `PushDotaLoginSync` / `MaybePrime…` / `PatchDotaLoginCache…` 声明
- `callback_client_welcome` / `callback_server_welcome` 与 stub 内联定义冲突
- 全量 coordinator 还拉 VPK unlock、login sync、prime cache、protobuf 生产路径

**结论：** 在可维护 offline 源列表内 **无法** 构造生产 `Steam_Game_Coordinator` 双实例走真 `SendMessage_`（档 A）。档 B（整 `Steam_Client`）成本更高，本轮不升级。

**延期条款命中：** `GP10_L4_HARNESS.md` §8 第 1 条（链接 spike 失败）。

GP-10 状态保持 **缺实现**；设计文档保留为 E5 准入依据。

## 3. GP-09 L2 延期（同源）

| 级别 | 状态 |
|------|------|
| L1 payload extract/build | 有（payload_helpers_test） |
| L1.5 pure plan | 有：`plan_client_hello` + `plan_server_hello` + lobby_flow_test；生产已消费 |
| L2 handler smoke | **延期**：`welcome_coordinator` 审计标记 *not directly offline-buildable*；与 S0 同链 |

**不为测大拆：** 不把 welcome 整文件硬链进 smoke；不抽第二套假 L4。

## 4. 已交付（E0–E4）

| 切片 | 交付 |
|------|------|
| E0 | GOLDEN / PHASE_E 分档 |
| E1 | ClientHello plan + 生产 + flow 测 |
| E2 | ServerHello plan + 生产 + flow 测 |
| E3 | 未做（可选，无回归痛） |
| E4 | `GP10_L4_HARNESS.md` |
| E5 | **延期**（S0 失败） |

## 5. Phase E 出口判定

**Phase E 主线关闭：** 测补强边界、GP-09 plan、GP-10 设计与延期均已落地。

**仍开放（非 E 阻塞）：**

- GP-09 L2 / GP-10 L4 实现（需可维护链接策略或生产可测 seam，另立项）
- Phase D 真 DI / 上帝类（默认不实施）
- local/shared 双轨（CURRENT 风险 #2）

## 6. 验证

```bash
bash tools/run_gc_verification.sh --full
```

S0 探测命令（预期失败，仅作证据复现）：

```bash
# stub 包装 include welcome_coordinator.cpp → 编译错误（缺 Hello 成员声明等）
```
