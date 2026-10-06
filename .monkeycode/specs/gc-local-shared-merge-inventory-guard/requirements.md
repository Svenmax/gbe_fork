# Local/shared Merge Inventory Guard 需求

## Introduction

本规格启动 Local/shared 双轨状态债的低风险收敛。第一步不迁移生产写入路径，只把 `LOCAL_LOBBY_USAGE.md` 中记录的剩余 merge/restore 字段组与生产代码入口建立可执行审计，避免后续字段组和文档漂移。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 Local/shared merge 与 restore 入口有可审计清单，以便后续一次只收敛一个字段组。

#### Acceptance Criteria

1. WHEN the audit scans production Local/shared merge owners, it SHALL collect the documented merge/restore entrypoints.
2. WHEN the audit scans `LOCAL_LOBBY_USAGE.md`, it SHALL collect the corresponding inventory entries.
3. WHEN a documented merge/restore entrypoint is removed from code or documentation, the audit SHALL fail.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望本切片只增加护栏，以便为后续字段级收敛提供稳定边界。

#### Acceptance Criteria

1. WHEN the implementation is complete, production Local/shared behavior SHALL remain unchanged.
2. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
3. WHEN the implementation is complete, `git diff --check` SHALL pass.
