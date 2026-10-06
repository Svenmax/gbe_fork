# Registry Metadata Inventory Guard 需求

## Introduction

本规格扩展 Dota GC production registry 与 `MESSAGE_ROUTING_INVENTORY.md` §1 的同步审计。上一切片已保护 emsg 集合，本切片继续保护每个 registry row 的 HandlerId、modes 与 lifecycle 元数据，避免真相表仅保留消息号但语义漂移。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 routing inventory §1 的 HandlerId、modes、lifecycle 与 production kTable 对齐，以便路由真相表能准确指导后续修改。

#### Acceptance Criteria

1. WHEN the audit scans production kTable, it SHALL collect each entry's HandlerId, RequestMode, and LifecycleClass.
2. WHEN the audit scans `MESSAGE_ROUTING_INVENTORY.md` §1, it SHALL collect each row's HandlerId, modes, and lifecycle.
3. WHEN any common emsg has different metadata between production and inventory, the audit SHALL fail.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望 existing emsg-set guard 继续保留，以便 metadata guard 增强现有保护而不削弱集合同步。

#### Acceptance Criteria

1. WHEN kTable and inventory emsg sets differ, the audit SHALL continue to fail.
2. WHEN kTable and inventory metadata differ, the audit SHALL fail with a focused metadata message.
3. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
4. WHEN the implementation is complete, `git diff --check` SHALL pass.
