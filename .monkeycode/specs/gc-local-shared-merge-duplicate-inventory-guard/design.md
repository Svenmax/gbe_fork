# Local/shared Merge Duplicate Inventory Guard 设计

Feature Name: gc-local-shared-merge-duplicate-inventory-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片让 `audit_local_shared_merge_inventory()` 在构建 documented set 前跟踪已见三元组，并对重复行输出明确诊断。变更范围限定在 audit、audit test 和状态文档。

## Components and Interfaces

- `tools/_audit_gc_refactor.py`: 在 Local/shared merge inventory 解析阶段检测重复三元组。
- `tools/test_audit_gc_refactor.py`: 增加重复清单行回归。
- `docs/gc/ACTIVE_QUEUE.md`、`docs/gc/CURRENT.md`: 同步护栏状态。

## Correctness Properties

1. 重复的 entrypoint、owner、field-group 三元组触发 audit 失败。
2. 唯一清单继续通过 audit。
3. 生产 C++ 文件保持不变。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
