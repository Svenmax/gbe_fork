# Local/shared Merge Duplicate Inventory Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 让 `audit_local_shared_merge_inventory()` 检测重复 inventory 三元组。
- [x] 增加重复清单行回归测试。
- [x] 同步 `docs/gc/CURRENT.md` 状态。
- [x] 运行 `python3 tools/test_audit_gc_refactor.py`。
- [x] 运行 `python3 tools/_audit_gc_refactor.py`。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] Local/shared merge inventory 重复行由 audit 保护。
- [x] production C++ 文件不变。
- [x] 全量 GC 验证和 diff check 通过。
