# Postgame Chat Tombstone 后续任务清单

## 已完成

- [x] 盘点 `abandon_pre_postgame_chat_channel_id` 的写入、读取、消费和清理调用点。
- [x] 定义 postgame action 中旧 chat channel tombstone 的数据与生命周期边界。
- [x] 收敛旧 channel payload 的匹配与消费入口，保持 postgame push 和 deferred reset 顺序。
- [x] 增加 old channel、postgame channel、7272/7014 cleanup、generation 竞争和新 lobby 回归。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] 旧 chat tombstone 的建立、消费和 generation-scoped 清理具有 focused 与 smoke 覆盖。
- [x] 全量 GC 验证和 diff check 通过。

## 结果

- `PostGameLobbyStateApply` 建立显式 `postgame_chat_tombstone_*` 字段，同时保持旧 `abandon_pre_postgame_chat_channel_id` 日志和兼容语义。
- `postgame_chat_tombstone_matches()` 统一 old-channel 7272 与 7014 retrieval 的 generation-scoped 匹配。
- 当前 postgame channel leave 经 `clear_postgame_chat_tombstone()` 清理 tombstone 与兼容字段；旧 generation tombstone payload 只返回 7014，不触发 abandon finalize。
- handler smoke 增至 `92/92`；`bash tools/run_gc_verification.sh --full` 与 `git diff --check` 均通过。
