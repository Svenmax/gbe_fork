# GC 重构后续深化需求

## Introduction

本需求文档定义下一轮 Dota GC 架构重构工作。上一轮已经完成 staged coordinator cleanup、DTO/decision 边界、基础 dependency seam、验证 wrapper 和治理文档。本轮目标是在保持协议行为稳定的前提下，继续收缩共享 header、扩大副作用 seam、推进 action plan executor、封装剩余 runtime state，并补齐生产构建与真实路径验证。

## Glossary

- **GC 重构系统**：`gbe_fork` 中 Dota GC handler、coordinator、payload helper、wire helper、测试和治理文档组成的重构范围。
- **Pure Helper**：只读取显式输入并返回结构化结果的函数，不写文件、不发送网络、不修改 coordinator/global state。
- **Executor**：在 coordinator 层按显式顺序执行真实副作用的函数。
- **Dependency Seam**：封装文件保存、server GC forward、network broadcast、lobby publish、snapshot refresh 等外部副作用的命名函数边界。
- **Verification Wrapper**：`tools/run_gc_verification.sh`，用于编排 offline tests、audit 和 style gate。

## Requirements

### Requirement 1: Production Build And Runtime Validation

**User Story:** AS a maintainer, I want the next GC refactor to validate production build entrypoints and representative runtime chains, so that shell-only test success does not hide integration issues.

#### Acceptance Criteria

1. WHEN a Premake-capable environment is available, the GC 重构系统 SHALL run `premake5 --with-gc-tests gmake2` and record the result in the task list.
2. WHEN Premake tooling is unavailable, the GC 重构系统 SHALL record the environment limitation and keep shell verification green through `tools/run_gc_verification.sh`.
3. WHEN validating runtime-sensitive chains, the GC 重构系统 SHALL cover `7035 abandon current game`, `7004 signout`, `8052/8053 custom game loading`, `2569 equip items full forward`, and reconnect/direct-connect flow in either focused tests or manual validation notes.

### Requirement 2: Internal Header Boundary Reduction

**User Story:** AS a contributor, I want large GC internal declarations split into narrower headers, so that domain files depend only on the helper surfaces they use.

#### Acceptance Criteria

1. WHEN moving wire payload declarations, the GC 重构系统 SHALL place DTO parser, proto patch, and template patch declarations behind a narrow wire-oriented header.
2. WHEN moving lobby payload declarations, the GC 重构系统 SHALL place practice lobby details, cache subscribed, launch payload, and chat payload declarations behind a narrow lobby-oriented header.
3. WHEN moving replay/template declarations, the GC 重构系统 SHALL keep canned payload ownership in template/replay or payload helper files and keep ordinary handler files free of large hex blobs.
4. IF declarations are moved between headers, the GC 重构系统 SHALL run `python3 tools/_audit_gc_refactor.py` and `tools/run_gc_verification.sh`.

### Requirement 3: Dependency Seam Expansion

**User Story:** AS a maintainer, I want high-risk external side effects behind named seams, so that pure planners and decisions remain side-effect free.

#### Acceptance Criteria

1. WHEN server GC forwarding is triggered by a Dota handler, the GC 重构系统 SHALL route the operation through a coordinator-level seam that preserves the current action order.
2. WHEN network broadcast is triggered by a Dota handler, the GC 重构系统 SHALL route the operation through a coordinator-level seam that records the business reason.
3. WHEN lobby publish, details update, or snapshot refresh is triggered by a Dota handler, the GC 重构系统 SHALL route the operation through a named seam or a documented executor step.
4. IF a pure helper needs an external side effect, the GC 重构系统 SHALL return an intent object or action entry for the executor to process.

### Requirement 4: Action Plan Executor Progression

**User Story:** AS a maintainer, I want `EquipItemsPlan` execution centralized, so that the action-list architecture becomes enforceable rather than descriptive.

#### Acceptance Criteria

1. WHEN `GBE_HandleDotaEquipItemsRequest` produces an `EquipItemsPlan`, the GC 重构系统 SHALL execute SO update, local response, item persistence, server GC forward, network broadcast, and snapshot refresh through an explicit executor function.
2. WHILE executing `EquipItemsPlan`, the GC 重构系统 SHALL preserve the current order observed by handler smoke tests.
3. IF executor extraction changes helper boundaries, the GC 重构系统 SHALL keep the planner free of coordinator, file, network, server GC, and lobby snapshot calls.
4. WHEN executor extraction completes, the GC 重构系统 SHALL add or update focused tests that fail on reordered save, server forward, broadcast, or snapshot refresh actions.

### Requirement 5: Runtime And Global State Encapsulation

**User Story:** AS a contributor, I want remaining mutable GC state accessed through functions, so that future state storage changes avoid broad handler edits.

#### Acceptance Criteria

1. WHEN encapsulating launch or sync flags, the GC 重构系统 SHALL add function-level accessors before moving storage.
2. WHEN encapsulating the host showcase flag, the GC 重构系统 SHALL preserve `7034` showcase repush behavior and add focused coverage or audit coverage.
3. WHEN encapsulating the server hello cache, the GC 重构系统 SHALL preserve direct server hello parse and welcome replay behavior.
4. IF evaluating a `DotaGcRuntimeState` storage struct, the GC 重构系统 SHALL keep existing accessors stable and migrate only one small state group at a time.

### Requirement 6: Routing And Registry Simplification

**User Story:** AS a maintainer, I want simple post-login handlers registered consistently, so that direct/wrapped dispatch logic remains auditable.

#### Acceptance Criteria

1. WHEN migrating a simple post-login handler into the routing registry, the GC 重构系统 SHALL preserve direct/wrapped path guards, job/session propagation, and fallback behavior.
2. WHEN adding registry entries, the GC 重构系统 SHALL update `_audit_gc_refactor.py` dispatch expectations.
3. IF a handler has complex template replay, launch chain, or fallback semantics, the GC 重构系统 SHALL keep that handler on an explicit path until focused tests cover the behavior.

### Requirement 7: Verification And Audit Hardening

**User Story:** AS a maintainer, I want verification and audit checks to enforce new architecture rules, so that future regressions are caught before review.

#### Acceptance Criteria

1. WHEN adding a new `.cpp` file for GC code, the GC 重构系统 SHALL verify source-list inclusion for shell tests and optional Premake test targets where applicable.
2. WHEN adding high-risk side effects to handler files, the GC 重构系统 SHALL either place them behind an approved seam or update audit allowlists with a documented reason.
3. WHEN changing reason strings, the GC 重构系统 SHALL update reason governance docs and focused tests that assert stable reasons.
4. WHEN finishing a task, the GC 重构系统 SHALL run `tools/run_gc_verification.sh` and record the result in the task list.

## Non-Goals

1. Rewrite every Dota GC handler in one pass.
2. Introduce a generic command framework or large class hierarchy for all handler actions.
3. Move `GBE_local_lobby` or `GBE_shared_dota_lobby_state` into a broad runtime struct before narrower state groups are stable.
4. Replace canned payload bytes without focused parser or replay fixture coverage.
