# Local/shared Merge Definition Guard 设计

Feature Name: gc-local-shared-merge-definition-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片把 `audit_local_shared_merge_inventory()` 的生产 owner 检查从普通文本匹配收紧为函数定义匹配。该变更只影响审计可信度，不修改 C++ production 行为。

## Components and Interfaces

- `tools/_audit_gc_refactor.py`: 增加 Local/shared merge entrypoint definition matcher，并在 inventory audit 中使用。
- `tools/test_audit_gc_refactor.py`: 增加 entrypoint 只作为调用点出现时仍失败的回归测试。
- `docs/gc/ACTIVE_QUEUE.md`、`docs/gc/CURRENT.md`、`.monkeycode/docs/ARCHITECTURE.md`: 同步当前护栏语义。

## Correctness Properties

1. 受保护 entrypoint 必须在 owner 文件中以函数定义形式存在。
2. 调用点、声明和注释中的 entrypoint 文本不能满足 production presence 检查。
3. 生产 Local/shared restore 或 merge 行为保持不变。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
