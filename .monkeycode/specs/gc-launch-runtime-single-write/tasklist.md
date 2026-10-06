# GC Launch Runtime 单写入任务清单

## 双轨状态收敛

- [x] 将 generic capture 的 `custom_game.*` 写入收敛为 pure plan/apply，并完成完整 GC 验证。
- [x] 将 generic metadata 读取组合为 `GenericLobbyCapturePlan`，提供一次 Local apply 边界。
- [x] 为 cache、replay 与 details payload 调用引入统一 capture facade，保持现有同步和 owner repair 语义。
- [x] 为 cache、replay 与 details snapshot 调用实现纯 snapshot projection，并补充 owner/member/custom runtime 回归。
- [x] 将 host capture 到 shared Store 的同步点收敛为显式 coordinator 边界。
- [x] 为 runtime identity 与 launch state 建立 source-aware shared restore plan。
- [x] 记录 generic metadata publish、8052 lifecycle pre-write 与 postgame chat tombstone 的独立后续规格。
