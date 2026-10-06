# GC 维护加固周期需求

## 目标

本周期以降低后续 Dota GC 更新维护风险为目标。工作应提高生产路径回归发现能力、并发验证可信度和共享状态安全性，不以代码外观、文件数量或架构形式统一为目标。

## 需求

### R1 测试断言在所有构建模式下有效

- 当生命周期状态机测试以 Debug、Release 或 `NDEBUG` 构建时，所有运行时检查均应执行。
- 当任一检查失败时，测试进程应返回非零状态。

### R2 共享 Store 多写者场景接受 ThreadSanitizer 验证

- PR TSAN 门禁应执行现有 Lobby Store 多写者、陈旧写入和 clear/publish 并发场景。
- 发现数据竞争时，门禁应失败。

### R3 完整 GC 行为测试进入 PR 阻断门禁

- Pull Request 应执行完整 GC offline suite。
- 完整测试可拆分为独立作业，但任一失败均应阻断合并。

### R4 生产 Post-login Dispatcher 获得行为级集成覆盖

- 测试应链接生产 registry factory、dispatcher 和 adapter 实现。
- 测试应验证 direct、wrapped、session forwarding、direct-only 和 unknown request 行为。
- 测试应执行真实 handler，并使用现有记录型外部依赖观察结果。

### R5 一个关键 Lobby 生命周期获得行为 Replay

- Replay 应连续执行 Create、Launch、Leave，并复用同一个 coordinator 状态。
- 输出应包含 handler、状态前后变化和有序副作用。
- 输出应与稳定 golden trace 比较。

### R6 Locator 和共享 Store 清理保持生命周期安全

- Dota Store 和 RuntimeState locator 应在构造失败时自动解绑。
- 生产共享 Store 清理应校验 generation，并保留单调 tombstone generation。
- 陈旧清理不得删除较新 Lobby 状态，陈旧发布不得复活已清理状态。

### R7 周期停止条件

- 完成本周期后进入维护模式。
- Composition Root 全面接管、状态机唯一权威和 Coordinator 深拆由实际功能更新触发。
- 文本架构审计仅随本周期生产边界变化同步，不扩展新的自证型门禁。
