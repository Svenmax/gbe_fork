# Requirements: Direct Local Lobby Const Alias Guard

## Scope

Keep audit 10d focused on mutable aliases while allowing const read-only aliases to `GBE_local_lobby`.

## Requirements

1. The audit shall accept read-only aliases shaped like `const auto& lobby = GBE_local_lobby`.
2. The audit shall continue rejecting mutable aliases shaped like `auto& lobby = GBE_local_lobby`.
3. Existing direct object, field, target pointer, compound, and mutating method diagnostics shall remain unchanged.
4. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
