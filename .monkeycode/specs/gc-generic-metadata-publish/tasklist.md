# Generic Metadata Publish 后续任务清单

## 已完成

- [x] 盘点 `GBE_CaptureCurrentDotaLobbyState()` 的所有调用点，并为每个调用点标注 host sync、client observe 或 pure projection 模式。
- [x] 将 host 权威 generic metadata publish 意图收敛到 `GBE_SyncCapturedDotaLobbyState()` 或等价的单一显式边界。
- [x] 验证 cache、replay 与 details payload 调用继续使用纯 projection。
- [x] 增加 host/client/generation/outbound-order focused 回归与 handler smoke。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] `docs/gc/LOCAL_LOBBY_USAGE.md` 记录最终 publish 决策与字段来源。
- [x] 全量 GC 验证和 diff check 通过。
