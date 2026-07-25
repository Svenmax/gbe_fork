# Local/shared Merge Duplicate Inventory Guard 需求

## Introduction

本规格继续加固 Local/shared merge inventory audit。当前审计以 set 比较文档清单，重复表格行会被折叠。本切片显式检测重复三元组，防止清单出现重复 entrypoint 但 audit 仍通过。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 Local/shared merge inventory 中的重复行被审计发现，以便清单保持唯一且可维护。

#### Acceptance Criteria

1. WHEN the inventory contains the same entrypoint, owner, and field-group tuple more than once, the audit SHALL fail.
2. WHEN the inventory contains each expected tuple once, the audit SHALL pass duplicate detection.
3. WHEN duplicate detection fails, the diagnostic SHALL identify the duplicated entrypoint, owner, and field group.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望本切片只提升文档清单护栏，以便 production 行为保持稳定。

#### Acceptance Criteria

1. WHEN the implementation is complete, production C++ files SHALL remain unchanged.
2. WHEN the implementation is complete, `python3 tools/test_audit_gc_refactor.py` SHALL pass.
3. WHEN the implementation is complete, `python3 tools/_audit_gc_refactor.py` SHALL pass.
4. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
5. WHEN the implementation is complete, `git diff --check` SHALL pass.
