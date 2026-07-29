# GC Internal Header Shrink Plan

This document classifies the remaining declarations in `dll/gbe_dota_gc_internal.h` and records the preferred direction for shrinking the internal shared surface. The goal is to avoid turning the header into a writable cross-file global bucket.

## Baseline

- `dll/gbe_dota_payload_item_helpers.h` already owns item payload declarations for equip op parsing, style bitmask mutation, SO object construction, and item serialization.
- `dll/gbe_dota_payload_lobby_helpers.h` and `dll/gbe_dota_payload_wire_helpers.h` already own many payload helper declarations.
- `dll/gbe_dota_inventory_ports.h` owns free equip ports (C.9); `gc_internal` must not re-declare them.
- D.12.2: `gbe_dota_gc_internal.h` no longer re-exports payload-lobby, locator, or reconnect headers; callers include those headers directly. Audit 5b enforces this.
- D.12.1: template-replay canned hex/byte arrays live in `gbe_dota_template_replay_templates.{h,cpp}`; the handler TU only dispatches.
- `dll/gbe_dota_gc_internal.h` still carries logging, old-ID const tables, `ser_var`/`deser_var`, hello accessors, and a small set of practice-lobby official hex tables.
- R3 已将日志、launch phase 描述、CacheSubscribed 摘要和协议边界日志迁至 `dll/gbe_dota_gc_diagnostics.h`。`gbe_dota_lobby_launch_coordinator.cpp` 与 `gbe_dota_gc_payload_helpers.cpp` 已移除对内部总线头的依赖。
- R3 已将 ServerHello cache、VPK loot cache 和二进制序列化模板分别迁至 `dll/gbe_dota_server_hello_cache.h`、`dll/gbe_dota_vpk_loot_cache.h`、`dll/gbe_dota_binary_helpers.h`。欢迎流、模板回放、库存协调器和 payload helper 通过这些窄接口访问对应能力。
- R3 已将旧 ID 替换字节、Practice Lobby 固定模板和官方十六进制资产迁至 `dll/gbe_dota_protocol_assets.h`。wire、payload 和 lobby helper 模块通过该只读协议资产接口访问常量。
- R3 已将 persona 外设消息构造迁至 `dll/gbe_dota_payload_wire_helpers.h`，并将响应包诊断迁至 `dll/gbe_dota_gc_diagnostics.h`。`dll/gbe_dota_gc_internal.h` 已无业务声明，所有源码和测试 include 已移除；保留的迁移标记可在专门的删除提交中清理。

## Logging And Trace

Keep in `gbe_dota_gc_internal.h` for now:

- `GBE_GC_DebugLog`
- `GBE_DescribeDotaLaunchPhase`
- `GBE_LogDotaSOCacheSubscribedSummary`
- `GBE_LogDotaResponsePacket`
- `GBE_LogGCProtoBoundary`

Reason: these are cross-cutting diagnostics used by multiple production TUs and tests. A future `gbe_dota_gc_logging.h` would be cleaner, but moving logging alone is low risk and can be handled later.

## Shared Mutable State

Current state:

- `GBE_vpk_loot_data` no longer has an `extern` declaration. Mutation goes through `GBE_SetDotaVpkLootData(...)`; reads go through `GBE_GetDotaVpkLootData()`.
- `GBE_last_dota_server_hello_context` no longer has an `extern` declaration. It is TU-local to `steam_game_coordinator.cpp`; external users use server-hello accessors.
- Shared lobby backing state is private to the Store implementation; production access is via `GBE_GetSharedDotaLobbyStateStore()` from `gbe_dota_locator.h` (not re-exported by `gc_internal` after D.12.2).
- Generation-gated Store writes are required in production (audit 10b). Bare `publish`/`update` remain test/fixture-only.

Further shrink of Store-facing helpers should only happen when a new lifecycle path needs a named operation; do not reopen mechanical getter/setter renames for appearance.

## Const Tables And Serialization Templates

Keep temporarily:

- Old account/SteamID/lobby ID replacement byte arrays in `gc_internal`.
- Practice-lobby official hex / cache-subscribed template tables still declared from the internal surface.
- `ser_var` / `deser_var` inline templates.

Template-replay canned protocol assets (D.12.1) already left the handler logic TU. Further moves of remaining const tables are optional and must keep `sizeof`/include self-containment green under `tools/run_gc_verification.sh`.

## Hello And Welcome Context

The context structs and accessors are wire-facing but stateful:

- `GBE_DotaHelloContext`
- `GBE_DotaServerHelloContext`
- direct hello extraction/build helpers
- server-hello cache accessors

Preferred direction:

- Keep structs near wire/payload helper declarations until welcome ownership is stable.
- Keep cache storage private to `steam_game_coordinator.cpp`.
- Do not reintroduce direct `extern` access to the cache variable.

## Lobby Payload Builders

Preferred direction:

- Lobby payload builders belong in `gbe_dota_payload_lobby_helpers.h` when they are pure or close to pure.
- Stateful publish/finalize/reset behavior belongs in coordinator/state files, not payload helper headers.
- When a function needs both payload construction and coordinator mutation, split planning/building from execution where practical.

## Migration Rules

- New mutable state should start as private to one owning TU.
- If multiple TUs need read access, expose a const accessor or snapshot helper first.
- If multiple TUs need mutation, expose named operations that describe intent instead of returning a mutable reference.
- Do not add `extern` mutable state to `gbe_dota_gc_internal.h` unless there is no smaller seam and the debt is documented in this file.
- Keep verification green with `tools/run_gc_verification.sh --full` after each shrink step.
