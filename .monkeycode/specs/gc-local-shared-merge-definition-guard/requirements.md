# Local/shared Merge Definition Guard 需求

## Introduction

本规格继续加固 Local/shared merge inventory audit。当前审计会确认受保护 entrypoint 名称出现在生产 owner 文件中；本切片将该检查收紧为函数定义级匹配，避免注释、声明或调用点让已移除的实现误通过。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 Local/shared merge inventory 校验真实函数定义，以便生产 restore/merge 入口被移除时审计立即失败。

#### Acceptance Criteria

1. WHEN an expected merge entrypoint has a function definition in the documented owner, the audit SHALL pass that entrypoint.
2. WHEN an expected merge entrypoint only appears as a call, declaration, or text reference, the audit SHALL fail that entrypoint.
3. WHEN an expected merge entrypoint definition is renamed or removed, the audit SHALL fail that entrypoint.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望本切片保持 production 行为稳定，以便只提升护栏可信度。

#### Acceptance Criteria

1. WHEN the implementation is complete, production C++ files SHALL remain behaviorally unchanged.
2. WHEN the implementation is complete, `python3 tools/test_audit_gc_refactor.py` SHALL pass.
3. WHEN the implementation is complete, `python3 tools/_audit_gc_refactor.py` SHALL pass.
4. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
5. WHEN the implementation is complete, `git diff --check` SHALL pass.
