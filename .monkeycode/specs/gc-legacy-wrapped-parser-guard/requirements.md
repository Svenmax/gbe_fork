# Legacy Wrapped Parser Guard 需求

## Introduction

本规格为 Dota GC wrapped 解析层职责添加审计护栏。`docs/gc/MESSAGE_ROUTING_INVENTORY.md` 已将 `GBE_ExtractWrappedDotaDirectContext` 标记为 `LEGACY_UNUSED`，生产 wrapped post-login 路径应统一走 `gbe_dota_gc_router.*` 的 `extract_wrapped_post_login_request()`。本切片只增加审计和文档状态，不改变生产路由行为。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 legacy wrapped direct parser 保持无生产调用，以便 wrapped 入站职责不会重新分叉。

#### Acceptance Criteria

1. WHEN production GC source files are audited, the audit SHALL fail if they call `GBE_ExtractWrappedDotaDirectContext`.
2. WHEN the parser remains only declared and defined in `gbe_dota_request_router.h`, the audit SHALL pass.
3. WHILE wrapped post-login requests are processed, the production path SHALL remain `extract_wrapped_post_login_request()`.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望 routing inventory 的 `LEGACY_UNUSED` 状态由工具保护，以便后续修改必须同步真相表。

#### Acceptance Criteria

1. WHEN `GBE_ExtractWrappedDotaDirectContext` is documented as `LEGACY_UNUSED`, the audit SHALL require the production no-call contract.
2. WHEN the routing inventory drops the `LEGACY_UNUSED` marker, the audit SHALL fail.
3. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
4. WHEN the implementation is complete, `git diff --check` SHALL pass.
