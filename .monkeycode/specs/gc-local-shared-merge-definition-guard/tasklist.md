# Local/shared Merge Definition Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 增加 Local/shared merge entrypoint 函数定义匹配 helper。
- [x] 将 `audit_local_shared_merge_inventory()` 的 owner 检查改为定义匹配。
- [x] 增加调用点误判回归测试。
- [x] 运行 `python3 tools/test_audit_gc_refactor.py`。
- [x] 运行 `python3 tools/_audit_gc_refactor.py`。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] Local/shared merge entrypoint presence 由函数定义级 audit 保护。
- [x] production 行为不改变。
- [x] 全量 GC 验证和 diff check 通过。
