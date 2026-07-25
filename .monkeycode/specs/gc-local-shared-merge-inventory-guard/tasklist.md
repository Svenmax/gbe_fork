# Local/shared Merge Inventory Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 在 `LOCAL_LOBBY_USAGE.md` 增加 Local/shared merge inventory 表。
- [x] 增加 Local/shared merge inventory audit。
- [x] 增加文档漂移和代码入口漂移回归测试。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] Local/shared merge entrypoints 由 audit 保护。
- [x] production 行为不改变。
- [x] 全量 GC 验证和 diff check 通过。
