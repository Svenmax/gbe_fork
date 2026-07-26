# Requirements: Direct Local Lobby Explicit Type Alias Guard

## Scope

Tighten audit 10d so explicit `GBE_LocalLobby` mutable aliases to `GBE_local_lobby` cannot bypass the alias guard.

## Requirements

1. The audit shall reject `GBE_LocalLobby& lobby = GBE_local_lobby` in production `.cpp` sources.
2. The audit shall reject `GBE_LocalLobby* lobby = &GBE_local_lobby` in production `.cpp` sources.
3. Existing `auto` and `GBE_DotaLobbyState` mutable alias diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
