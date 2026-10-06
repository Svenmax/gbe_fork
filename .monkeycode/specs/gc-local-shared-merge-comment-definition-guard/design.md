# Local/shared Merge Comment Definition Guard 设计

Feature Name: gc-local-shared-merge-comment-definition-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片让 Local/shared merge entrypoint 的函数定义匹配复用既有 `strip_comments()`，先移除 C++ block comment 与 line comment，再执行定义匹配。生产 C++ 行为不变。

## Components and Interfaces

- `tools/_audit_gc_refactor.py`: `has_cpp_function_definition()` 在匹配前剥离注释。
- `tools/test_audit_gc_refactor.py`: 增加 block comment 伪定义回归。
- `docs/gc/ACTIVE_QUEUE.md`、`docs/gc/CURRENT.md`、`.monkeycode/docs/ARCHITECTURE.md`: 同步护栏语义。

## Correctness Properties

1. 注释中的函数文本不能满足 definition presence 检查。
2. 真实函数定义继续满足 definition presence 检查。
3. 生产 Local/shared restore 或 merge 行为保持不变。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
