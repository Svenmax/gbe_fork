# Requirements: Direct Local Lobby Decltype Auto Alias Guard

## Scope

Tighten audit 10d so parenthesized `decltype(auto)` aliases to `GBE_local_lobby` cannot bypass the mutable alias guard.

## Requirements

1. The audit shall reject `decltype(auto) lobby = (GBE_local_lobby)` in production `.cpp` sources.
2. Existing mutable alias diagnostics for `auto&`, `auto&&`, explicit Local types, and pointers shall remain unchanged.
3. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
