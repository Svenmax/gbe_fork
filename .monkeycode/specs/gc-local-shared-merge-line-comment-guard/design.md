# Local/shared Merge Line-comment Guard 设计

Feature Name: gc-local-shared-merge-line-comment-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片为 Local/shared merge definition audit 增加 line comment 伪定义回归测试。实现代码已通过 `strip_comments()` 剥离 `//` 注释；本切片只冻结该行为。

## Components and Interfaces

- `tools/test_audit_gc_refactor.py`: 增加 line comment 中伪函数定义不满足 entrypoint presence 的回归。
- `docs/gc/ACTIVE_QUEUE.md`、`docs/gc/CURRENT.md`: 同步测试护栏状态。

## Correctness Properties

1. `// bool restore_lobby_generation() { ... }` 这类文本不能满足 definition presence 检查。
2. 真实函数定义继续满足 definition presence 检查。
3. 生产 C++ 文件保持不变。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
