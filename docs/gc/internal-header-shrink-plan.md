# GC Internal Header Shrink Plan

This document classifies the remaining declarations in `dll/gbe_dota_gc_internal.h` and records the preferred direction for shrinking the internal shared surface. The goal is to avoid turning the header into a writable cross-file global bucket.

## Baseline

- `dll/gbe_dota_payload_item_helpers.h` already owns item payload declarations for equip op parsing, style bitmask mutation, SO object construction, and item serialization.
- `dll/gbe_dota_payload_lobby_helpers.h` and `dll/gbe_dota_payload_wire_helpers.h` already own many payload helper declarations.
- `dll/gbe_dota_gc_internal.h` still carries mixed declarations for logging, const tables, templates, shared state, hello context accessors, and stateful orchestration helpers.

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
- `GBE_shared_dota_lobby_state` remains directly exposed because it is used by handlers, payload helpers, state coordinators, and launch/finalize flows.

Next step for `GBE_shared_dota_lobby_state` should not be a mechanical getter/setter rename. Prefer a dedicated facade/context with named operations, for example:

- `GBE_HasSharedDotaLobbyState()`
- `GBE_GetSharedDotaLobbySnapshot(...)`
- `GBE_ClearSharedDotaLobbyState(reason)`
- `GBE_PublishLocalLobbyToShared(reason)`
- `GBE_GetSharedDotaLobbyIdOrZero()`

The facade should preserve existing side-effect ordering and logging reasons.

## Const Tables And Serialization Templates

Keep temporarily:

- Old account/SteamID/lobby ID replacement byte arrays.
- `GBE_kDotaOfficial*` template byte arrays.
- `ser_var` / `deser_var` inline templates.

These are widely used by patching, replay, and inventory code. Moving them is lower priority than removing mutable `extern` state.

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
