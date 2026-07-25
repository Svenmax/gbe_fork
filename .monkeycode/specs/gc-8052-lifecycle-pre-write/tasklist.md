# 8052 Lifecycle Pre-write 后续任务清单

## 已完成

- [x] 绘制 8052 direct 与 wrapped 从 parser 到 action executor 的写入顺序。
- [x] 定义并实现 8052 Local lifecycle pre-write 的单一 action 边界。
- [x] 保持 runtime queue 成功分支与 fallback publish/details 分支的现有条件顺序。
- [x] 覆盖 gate 拒绝、lobby 失配、queue 成功、queue 失败及 direct/wrapped 等价性。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] 8052 Local state、launch phase、runtime queue 和 fallback publish 的写入顺序有测试保护。
- [x] 全量 GC 验证和 diff check 通过。

## 结果

- `8052` direct 与 wrapped 均经 lifecycle action list 执行 `LocalLifecyclePreWrite`，在 runtime queue / fallback publish / details update 前一次写入 Local `state`、`game_state` 与 `launch_phase`。
- `RuntimeLobbyDetailsUpdate` 成功时继续跳过 fallback publish/details；失败时保持 shared publish 后 details update 的既有顺序。
- handler smoke 覆盖 direct/wrapped action sequence 等价、inactive / mismatched lobby gate、duplicate determinism、executor conditional action 与 failure policy。
- `bash tools/run_gc_verification.sh --full` 通过；`git diff --check` 通过。
