# Requirements: Direct Local Lobby Forwarding Alias Guard

## Scope

Tighten audit 10d so forwarding-reference aliases to `GBE_local_lobby` cannot bypass the mutable alias guard.

## Requirements

1. The audit shall reject `auto&& lobby = GBE_local_lobby` in production `.cpp` sources.
2. Existing mutable alias diagnostics for `auto&`, explicit Local types, and pointers shall remain unchanged.
3. Existing const alias behavior shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
