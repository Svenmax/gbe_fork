# Local/shared Merge Line-comment Guard 需求

## Introduction

本规格补齐 Local/shared merge comment definition guard 的测试覆盖。当前实现已经在 definition matcher 前剥离 C++ 注释；本切片增加 line comment 伪定义回归，确保注释剥离契约同时覆盖 `//` 注释。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 line comment 中的 Local/shared merge 伪定义被测试覆盖，以便审计不会因单行注释中的旧函数文本误通过。

#### Acceptance Criteria

1. WHEN an expected entrypoint appears only inside a line comment, the audit SHALL fail that entrypoint.
2. WHEN an expected entrypoint appears inside a real function definition, the audit SHALL pass that entrypoint.
3. WHEN the line-comment regression runs, the regression SHALL assert the existing definition-missing diagnostic.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望本切片只增加测试护栏，以便 production 行为保持稳定。

#### Acceptance Criteria

1. WHEN the implementation is complete, production C++ files SHALL remain unchanged.
2. WHEN the implementation is complete, `python3 tools/test_audit_gc_refactor.py` SHALL pass.
3. WHEN the implementation is complete, `python3 tools/_audit_gc_refactor.py` SHALL pass.
4. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
5. WHEN the implementation is complete, `git diff --check` SHALL pass.
