# Local/shared Merge Inventory Guard 设计

Feature Name: gc-local-shared-merge-inventory-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片为 Local/shared 双轨状态收敛建立第一层 inventory guard。审计从生产代码中确认关键 merge/restore/helper 入口存在，并与 `docs/gc/LOCAL_LOBBY_USAGE.md` 的清单保持同步。本切片不修改生产状态写入顺序。

## Components and Interfaces

- `docs/gc/LOCAL_LOBBY_USAGE.md`: 增加可审计的 Local/shared merge inventory 表。
- `tools/_audit_gc_refactor.py`: 增加 Local/shared merge inventory audit。
- `tools/test_audit_gc_refactor.py`: 增加文档漂移和代码入口漂移回归。

## Correctness Properties

1. 清单中的每个 entrypoint 都必须在生产代码中存在。
2. 生产代码中被纳入本切片的 entrypoint 都必须在清单中出现。
3. 本切片不改变任何 C++ production 行为。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
