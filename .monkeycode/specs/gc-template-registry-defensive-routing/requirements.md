# Template Registry Defensive Routing 需求

## Introduction

本规格收敛 Dota GC post-login 路由中的 template replay 防御转调。当前 `GBE_HandleDotaTemplateReplayRequest()` 同时承担 template-only canned/synthetic payload 与 registry-owned emsg 的防御转调；本切片只处理已由 production registry 拥有的 emsg，保持 template-only 行为、Hello/Welcome pipeline 与协议 payload 不变。

## Glossary

- **Registry-owned emsg**: 已在 `GBE_ProductionDotaHandlerRegistry()` 中注册并由 post-login dispatcher 负责的 emsg。
- **Registry-defensive case**: template replay switch 中用于兜底转调 registry-owned handler 的 case。
- **Template-only case**: template replay switch 中仍由 canned 或 synthetic payload 拥有的 case。
- **Routing inventory**: `docs/gc/MESSAGE_ROUTING_INVENTORY.md` 中的四轨路由真相表。

## Requirements

### Requirement 1

**User Story:** 作为 GC 维护者，我希望 registry-owned emsg 的 template replay 防御路径有显式边界，以便路由多轨债务可以逐步收敛。

#### Acceptance Criteria

1. WHEN template replay receives a registry-owned emsg, the Dota GC SHALL route the request through one explicit defensive boundary.
2. WHEN a registry-defensive emsg is added or removed, the routing inventory SHALL list the ownership and behavior.
3. WHILE registry owns an emsg, the Dota GC SHALL preserve the post-login dispatcher as the primary production path.

### Requirement 2

**User Story:** 作为 GC 维护者，我希望 template-only replay 行为保持稳定，以便 canned/synthetic payload 不受 registry 清理影响。

#### Acceptance Criteria

1. WHEN template replay receives a template-only emsg, the Dota GC SHALL preserve the existing canned or synthetic response behavior.
2. WHEN template replay misses all known cases, the Dota GC SHALL preserve the existing no-template miss behavior.
3. WHILE Hello or ServerHello messages are processed, the Dota GC SHALL preserve the welcome coordinator path.

### Requirement 3

**User Story:** 作为 GC 维护者，我希望该切片具备路由回归护栏，以便后续删除防御路径时有可靠证据。

#### Acceptance Criteria

1. WHEN registry-defensive routing changes, the Dota GC SHALL keep post-login registry audit green.
2. WHEN template replay ownership changes, the Dota GC SHALL keep template/replay fixture behavior green.
3. WHEN the implementation is complete, `bash tools/run_gc_verification.sh --full` SHALL pass.
4. WHEN the implementation is complete, `git diff --check` SHALL pass.
