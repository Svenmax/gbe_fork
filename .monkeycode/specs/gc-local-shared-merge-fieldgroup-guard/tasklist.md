# Local/shared Merge Field-group Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 扩展 `LOCAL_SHARED_MERGE_ENTRYPOINTS` 为 entrypoint、owner、字段组三元组。
- [x] 扩展 `audit_local_shared_merge_inventory()` 校验字段组漂移。
- [x] 增加字段组漂移回归测试。
- [x] 运行 `python3 tools/test_audit_gc_refactor.py`。
- [x] 运行 `python3 tools/_audit_gc_refactor.py`。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] Local/shared merge 字段组标签由 audit 保护。
- [x] production 行为不改变。
- [x] 全量 GC 验证和 diff check 通过。
