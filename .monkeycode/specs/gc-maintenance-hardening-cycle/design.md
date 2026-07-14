# GC 维护加固周期技术设计

## 设计原则

- 保持 Steam-facing API、消息顺序和 wire payload 稳定。
- 复用现有 offline harness、ActionRecorder、Store 测试和 CI 脚本。
- 优先修改测试与窄生产边界，避免对象图和状态模型的大范围迁移。
- 每项生产改动必须由 focused regression 和完整验证支撑。

## 测试可信度

生命周期状态机测试使用始终生效的本地检查函数替代标准 `assert`。失败检查记录表达式和行号并立即终止，使 `NDEBUG` 不影响测试语义。

TSAN runner新增 `gbe_dota_lobby_state_store_test`，直接执行已有多写者、陈旧写入和 clear/publish 交错场景。PR GC verification 改为执行 full suite。

## Production Dispatcher 集成测试

将 production registry factory 和 dispatcher 从 `steam_game_coordinator.cpp` 原样提取到窄 production translation unit。生产构建和 handler harness 编译同一实现。

首批集成覆盖：

- Production registry 基本契约。
- `7009` wrapped 请求与 session forwarding。
- `7009` direct 请求与 session 隔离。
- `4523` direct-only 请求及 wrapped 拒绝。
- unknown 和 invalid context 拒绝。

## 行为 Replay

新增轻量 behavior replay executable，复用 handler harness 和 fixture parser。初始场景连续执行：

```text
PracticeLobbyCreate -> PracticeLobbyLaunch -> PracticeLobbyLeave
```

Trace 记录归一化状态和 ActionRecorder 副作用顺序。动态 Lobby ID 在输出层映射为稳定符号，不改变 handler 输入和内部状态。

## Locator 生命周期

新增组合 RAII binding guard。Guard 在 bind 第二项失败时回滚第一项，并在正常析构或构造异常时按逆序解绑。`Steam_Client` 在基础依赖创建完成、Dota coordinator 创建前绑定 locator。

## Generation-aware Clear

Store 新增 compare-and-clear 操作：

- 当前 generation 必须匹配 expected generation。
- 清空后保存 tombstone generation。
- tombstone generation 保持单调。
- 陈旧 clear 返回 `StaleGeneration` 且不修改状态。

生产 runtime clear 在覆盖本地 Lobby 前捕获 generation，并通过该操作清理共享 Store。

## 验证

每个阶段运行 focused test。最终运行：

```bash
bash tools/run_gc_offline_tests.sh --full
CXX=clang++ bash tools/run_gc_tsan_tests.sh
bash tools/run_gc_verification.sh --full
git diff --check
```

## 停止条件

上述门禁通过后结束本周期。后续仅在真实功能修改暴露具体维护阻力时推进 Composition Root、持久状态机或 Coordinator 对象拆分。
