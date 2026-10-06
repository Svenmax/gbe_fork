# Direct Conditional Fallback Routing 需求

## Introduction

本规格收敛 Dota GC direct post-login 路由中的条件 fallback。当前 `GBE_HandleDotaDirectPostLoginRequest()` 在 registry miss 后内联处理观察探针和 late-steam 条件 consume，再落到 template replay。本切片只抽出显式 direct conditional fallback boundary，保持 registry 主路径、template-only replay 和 wrapped hard-miss 行为不变。

## Glossary

- **Direct conditional fallback**: direct post-login registry miss 后、template replay 前的条件处理路径。
- **Conditional probe**: `8744` 只记录诊断，再继续进入 template replay。
- **Conditional consume**: `5410` 和 `5432` 在 late-steam tracking 开启时被消费并返回 handled。
- **Template-only catch-all**: 条件 fallback 未处理时继续进入 `GBE_HandleDotaTemplateReplayRequest()`。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 direct post-login 条件 fallback 有显式边界，以便 registry miss 后的路由职责可审计。

#### Acceptance Criteria

1. WHEN direct post-login registry misses, the Dota GC SHALL evaluate `8744`, `5410`, and `5432` through one explicit conditional fallback boundary.
2. WHEN `8744` is observed, the Dota GC SHALL preserve the existing diagnostic log and continue to template replay.
3. WHEN `5410` or `5432` is received while late-steam tracking is active, the Dota GC SHALL preserve the existing consume-and-return behavior.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望 fallback 收敛不改变其它路由轨道，以便 template replay 和 wrapped hard miss 行为保持稳定。

#### Acceptance Criteria

1. WHEN direct conditional fallback does not consume a request, the Dota GC SHALL continue to template replay with the same emsg, body, source job, and return behavior.
2. WHEN wrapped post-login registry misses, the Dota GC SHALL preserve hard-miss behavior.
3. WHILE registry owns an emsg, the Dota GC SHALL preserve `GBE_DispatchDotaPostLoginRequest()` as the primary production path.

### Requirement 3

**User Story:** 作为 GC 维护者，我希望该边界有审计回归，以便后续新增 direct fallback 必须先更新真相表。

#### Acceptance Criteria

1. WHEN direct conditional fallback routing changes, the routing inventory SHALL remain aligned with the implementation.
2. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
3. WHEN the implementation is complete, `git diff --check` SHALL pass.
