# Local/shared Merge Field-group Guard 设计

Feature Name: gc-local-shared-merge-fieldgroup-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片扩展 Local/shared merge inventory audit，使审计比较 `entrypoint`、`owner` 与 `字段组` 三元组。生产 C++ 行为不变，变更范围限定在文档真相表、审计脚本和审计测试。

## Components and Interfaces

- `docs/gc/LOCAL_LOBBY_USAGE.md`: 继续作为 Local/shared merge entrypoint 与字段组标签真相表。
- `tools/_audit_gc_refactor.py`: 将 `LOCAL_SHARED_MERGE_ENTRYPOINTS` 从二元组扩展为三元组，并比较字段组标签。
- `tools/test_audit_gc_refactor.py`: 增加字段组标签漂移回归。

## Correctness Properties

1. 每个受保护 entrypoint 的 owner 必须与文档一致。
2. 每个受保护 entrypoint 的字段组标签必须与文档一致。
3. 生产 Local/shared restore 或 merge 行为保持不变。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
