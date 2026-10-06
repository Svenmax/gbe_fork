# Local/shared Merge Comment Definition Guard 需求

## Introduction

本规格继续加固 Local/shared merge definition guard。函数定义级 audit 需要忽略注释内容，避免 block comment 中保留的旧函数文本让已删除的 production entrypoint 误通过。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 Local/shared merge definition audit 忽略注释中的伪定义，以便真实 production entrypoint 缺失时审计稳定失败。

#### Acceptance Criteria

1. WHEN an expected entrypoint appears only inside a block comment, the audit SHALL fail that entrypoint.
2. WHEN an expected entrypoint appears only inside a line comment, the audit SHALL fail that entrypoint.
3. WHEN an expected entrypoint has a real function definition, the audit SHALL pass that entrypoint.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望本切片只提升 audit 可信度，以便 production Local/shared 行为保持稳定。

#### Acceptance Criteria

1. WHEN the implementation is complete, production C++ files SHALL remain behaviorally unchanged.
2. WHEN the implementation is complete, `python3 tools/test_audit_gc_refactor.py` SHALL pass.
3. WHEN the implementation is complete, `python3 tools/_audit_gc_refactor.py` SHALL pass.
4. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
5. WHEN the implementation is complete, `git diff --check` SHALL pass.
