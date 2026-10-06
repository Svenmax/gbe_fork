# Template Only Inventory Guard 设计

Feature Name: gc-template-only-inventory-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片增加 `audit_template_only_inventory_guard`，将 template replay production switch 中的 `TEMPLATE_ONLY` emsg 集合与 `MESSAGE_ROUTING_INVENTORY.md` §4 的 `TEMPLATE_ONLY` 行对齐。审计只读取 production handler 和路由真相表，不改 handler 行为。

## Components and Interfaces

- `tools/_audit_gc_refactor.py`: 增加 expected `TEMPLATE_ONLY` emsg 集合、case 抽取和 inventory 抽取审计。
- `tools/test_audit_gc_refactor.py`: 增加 current pass、switch drift、inventory drift regression tests。
- `docs/gc/MESSAGE_ROUTING_INVENTORY.md`: 标记 template-only whitelist 由 audit 保护。

## Correctness Properties

1. `REGISTRY_DEFENSIVE` cases remain owned by `audit_registry_defensive_template_routing`.
2. `TEMPLATE_ONLY` cases remain an explicit fixed whitelist.
3. Inventory and production switch drift fail in focused audit tests.

## Test Strategy

- Run `python3 tools/test_audit_gc_refactor.py`.
- Run `python3 tools/_audit_gc_refactor.py`.
- Run `bash tools/run_gc_verification.sh --full`.
- Run `git diff --check`.
