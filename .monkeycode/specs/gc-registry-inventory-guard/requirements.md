# Registry Inventory Guard 需求

## Introduction

本规格为 Dota GC production registry 与 `MESSAGE_ROUTING_INVENTORY.md` §1 增加同步审计。`GBE_ProductionDotaHandlerRegistry()` 是 post-login 新消息入口，routing inventory 是协作真相表；两者的 emsg 集合漂移会削弱后续 Agent 的路由判断。本切片只增加审计和文档状态，不改变生产路由行为。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 production registry kTable 与 routing inventory §1 emsg 集合保持一致，以便新增或删除 registry entry 必须同步真相表。

#### Acceptance Criteria

1. WHEN the audit scans `GBE_ProductionDotaHandlerRegistry()`, it SHALL collect every registered emsg.
2. WHEN the audit scans `MESSAGE_ROUTING_INVENTORY.md` §1, it SHALL collect every registry inventory emsg.
3. WHEN the two emsg sets differ, the audit SHALL fail.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望 audit 自动解析 `GBE_k*` constants，以便 kTable 可以继续使用具名 protocol constants。

#### Acceptance Criteria

1. WHEN kTable entries use `GBE_k*` constants, the audit SHALL resolve them through `gbe_dota_protocol_constants.h`.
2. WHEN kTable entries use numeric literals, the audit SHALL compare their numeric value directly.
3. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
4. WHEN the implementation is complete, `git diff --check` SHALL pass.
