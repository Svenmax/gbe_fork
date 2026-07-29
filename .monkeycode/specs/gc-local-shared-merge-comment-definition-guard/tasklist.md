# Local/shared Merge Comment Definition Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 让 `has_cpp_function_definition()` 匹配前剥离 C++ 注释。
- [x] 增加 block comment 伪定义回归测试。
- [x] 运行 `python3 tools/test_audit_gc_refactor.py`。
- [x] 运行 `python3 tools/_audit_gc_refactor.py`。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] Local/shared merge definition audit 忽略注释中的伪定义。
- [x] production 行为不改变。
- [x] 全量 GC 验证和 diff check 通过。
