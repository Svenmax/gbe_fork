# Template Only Inventory Guard 需求

## Introduction

本规格为 Dota GC template replay 的 `TEMPLATE_ONLY` 白名单增加审计护栏。`docs/gc/MESSAGE_ROUTING_INVENTORY.md` §4 是 template catch-all 的真相表；生产 switch 中新增、删除或改归属 case 时必须同步该表。本切片只增加审计和文档状态，不改变生产路由行为。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 template replay switch 的 `TEMPLATE_ONLY` case 与 routing inventory 保持一致，以便 catch-all 路由不会无标注扩张。

#### Acceptance Criteria

1. WHEN the audit scans `GBE_HandleDotaTemplateReplayRequest`, it SHALL fail if the `TEMPLATE_ONLY` switch cases differ from the expected whitelist.
2. WHEN a new `TEMPLATE_ONLY` case is added to the production switch, the audit SHALL require a matching routing inventory update.
3. WHEN a `TEMPLATE_ONLY` inventory row is removed or reclassified, the audit SHALL fail.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望 registry-owned defensive template cases remain separate from the `TEMPLATE_ONLY` whitelist，以便 registry 主路径和 catch-all owner 不混淆。

#### Acceptance Criteria

1. WHEN `REGISTRY_DEFENSIVE` emsgs appear in the template-only switch body, the existing audit SHALL continue to fail.
2. WHEN `TEMPLATE_ONLY` emsgs appear only in the template-only switch body and inventory, the new audit SHALL pass.
3. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
4. WHEN the implementation is complete, `git diff --check` SHALL pass.
