# Requirements: Direct Local Lobby Pointer Alias Guard

## Scope

Lock audit 10d coverage for mutable pointer aliases to `GBE_local_lobby`.

## Requirements

1. The audit test suite shall explicitly reject mutable pointer aliases shaped like `GBE_DotaLobbyState* lobby = &GBE_local_lobby`.
2. The rejection shall use the existing mutable alias diagnostic.
3. The change shall not alter production Dota GC behavior or audit implementation semantics.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
