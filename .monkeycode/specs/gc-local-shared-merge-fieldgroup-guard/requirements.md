# Local/shared Merge Field-group Guard 需求

## Introduction

本规格延续 Local/shared merge inventory guard。当前审计已保护 entrypoint 与 owner；本切片把 `LOCAL_LOBBY_USAGE.md` 表格中的字段组列纳入可执行校验，避免后续字段组分类漂移影响后续一次一个字段组的收敛节奏。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 Local/shared merge entrypoint 的字段组标签有审计保护，以便后续字段级收敛保持稳定范围。

#### Acceptance Criteria

1. WHEN the audit scans `LOCAL_LOBBY_USAGE.md`, the audit SHALL collect each documented merge entrypoint, owner, and field-group label.
2. WHEN a documented field-group label drifts from the expected inventory, the audit SHALL fail.
3. WHEN an entrypoint, owner, and field-group label match the expected inventory, the audit SHALL pass.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望本切片只扩展文档护栏，以便保持生产 Local/shared 行为稳定。

#### Acceptance Criteria

1. WHEN the implementation is complete, production C++ files SHALL remain behaviorally unchanged.
2. WHEN the implementation is complete, `python3 tools/test_audit_gc_refactor.py` SHALL pass.
3. WHEN the implementation is complete, `python3 tools/_audit_gc_refactor.py` SHALL pass.
4. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
5. WHEN the implementation is complete, `git diff --check` SHALL pass.
