# Registry Inventory Guard 设计

Feature Name: gc-registry-inventory-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片增加 `audit_registry_inventory_guard`，将 `dll/gbe_dota_post_login_dispatcher.cpp` 中 production kTable 的 emsg 集合与 `docs/gc/MESSAGE_ROUTING_INVENTORY.md` §1 表格同步。审计解析 `gbe_dota_protocol_constants.h` 中的 `GBE_k*` 常量，并保留现有 dispatch 行为审计。

## Components and Interfaces

- `tools/_audit_gc_refactor.py`: 增加 constants 解析、registry kTable emsg 抽取和 inventory §1 emsg 抽取。
- `tools/test_audit_gc_refactor.py`: 增加 current pass、registry drift、inventory drift regression tests。
- `docs/gc/MESSAGE_ROUTING_INVENTORY.md`: 标记 registry inventory 由 audit 保护。

## Correctness Properties

1. 新增 registry entry 缺少 inventory 行时 audit 失败。
2. inventory §1 删除 registry emsg 行时 audit 失败。
3. 具名 constants 和 numeric literals 统一比较为十进制 emsg 字符串。

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
