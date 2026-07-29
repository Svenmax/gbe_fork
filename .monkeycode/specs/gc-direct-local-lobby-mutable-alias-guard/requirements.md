# Requirements: Direct Local Lobby Mutable Alias Guard

## Scope

Tighten audit 10d so mutable aliases to `GBE_local_lobby` cannot bypass direct field write detection.

## Requirements

1. The audit shall reject production `.cpp` mutable reference aliases shaped like `auto& lobby = GBE_local_lobby`.
2. The audit shall reject production `.cpp` mutable pointer aliases shaped like `auto* lobby = &GBE_local_lobby` or `GBE_DotaLobbyState* lobby = &GBE_local_lobby`.
3. Existing direct object, direct field, compound field, mutating method, and target pointer diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
