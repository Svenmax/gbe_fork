# Wrapped Hard Miss Routing 需求

## Introduction

本规格收敛 Dota GC wrapped post-login registry miss 的 hard-miss 边界。当前 `GBE_HandleDotaWrappedPostLoginRequest()` 在 registry miss 后内联记录诊断并返回 false。本切片只抽出显式 wrapped hard-miss boundary，保持 wrapped extract、supported request gate、registry 主路径、direct fallback 和 template replay 行为不变。

## Glossary

- **Wrapped hard miss**: wrapped post-login registry miss 后直接 log 并返回 false 的路径。
- **Registry miss**: `GBE_DispatchDotaPostLoginRequest()` 未处理当前 wrapped request。
- **Dead fallback**: 已删除的 wrapped miss 误落 SetTeamSlot 行为，必须保持缺席。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 wrapped registry miss 有显式 hard-miss 边界，以便 wrapped 路由不会重新滑回 dead fallback。

#### Acceptance Criteria

1. WHEN wrapped post-login registry misses, the Dota GC SHALL route the miss through one explicit hard-miss boundary.
2. WHEN the hard-miss boundary runs, the Dota GC SHALL preserve the existing diagnostic log fields and return false.
3. WHILE wrapped hard miss is handled, the Dota GC SHALL NOT route the request to template replay or SetTeamSlot fallback.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望 wrapped hard miss 收敛不改变其它路由轨道，以便 direct fallback 和 registry behavior 保持稳定。

#### Acceptance Criteria

1. WHEN wrapped extract fails or unsupported wrapped request is received, the Dota GC SHALL preserve the existing false return behavior.
2. WHEN registry handles a wrapped request, the Dota GC SHALL return true before the hard-miss boundary.
3. WHEN direct post-login registry misses, the Dota GC SHALL preserve direct conditional fallback and template replay behavior.

### Requirement 3

**User Story:** 作为 GC 维护者，我希望 wrapped hard miss 有审计回归，以便后续修改 wrapped miss 必须同步真相表。

#### Acceptance Criteria

1. WHEN wrapped hard miss routing changes, the routing inventory SHALL remain aligned with the implementation.
2. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
3. WHEN the implementation is complete, `git diff --check` SHALL pass.
