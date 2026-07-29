# Legacy Wrapped Parser Guard 设计

Feature Name: gc-legacy-wrapped-parser-guard
Updated: 2026-07-25
Status: Completed

## Description

本切片为 `GBE_ExtractWrappedDotaDirectContext` 的 `LEGACY_UNUSED` 状态增加审计。审计只扫描生产 GC 源文件调用点，允许该 inline parser 在 `gbe_dota_request_router.h` 中保留声明和定义。

## Components and Interfaces

- `docs/gc/MESSAGE_ROUTING_INVENTORY.md`: 保持解析层职责和 `LEGACY_UNUSED` 状态。
- `tools/_audit_gc_refactor.py`: 增加 legacy wrapped parser guard audit。
- `tools/test_audit_gc_refactor.py`: 增加 focused regression tests。

## Correctness Properties

1. Wrapped post-login production path remains `extract_wrapped_post_login_request()`.
2. `GBE_ExtractWrappedDotaDirectContext` remains available only as legacy inline parser code.
3. The audit ignores declaration/definition in `gbe_dota_request_router.h` and flags production call sites.

## Test Strategy

- Add audit tests for current pass, synthetic production call failure, and routing inventory marker drift.
- Execute `bash tools/run_gc_verification.sh --full`.
- Execute `git diff --check`.
