# Registry Metadata Inventory Guard 设计

Feature Name: gc-registry-metadata-inventory-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片扩展 `audit_registry_inventory_guard`，将 production kTable 中的 HandlerId、RequestMode、LifecycleClass 标准化后，与 `MESSAGE_ROUTING_INVENTORY.md` §1 的 HandlerId、modes、lifecycle 对齐。审计仍读取同一 registry owner 和 routing inventory，不改 production 路由行为。

## Components and Interfaces

- `tools/_audit_gc_refactor.py`: 扩展 registry row 解析，新增 metadata mismatch 检查。
- `tools/test_audit_gc_refactor.py`: 增加 production metadata drift 与 inventory metadata drift regression tests。
- `docs/gc/MESSAGE_ROUTING_INVENTORY.md`: 标记 registry emsg 与 metadata 都由 audit 保护。

## Correctness Properties

1. `DirectAndWrapped` 在 audit 中映射为 inventory 的 `D+W`。
2. `Direct`、HandlerId、LifecycleClass 与 inventory 文本逐项比较。
3. emsg set drift 和 metadata drift 都会失败。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
