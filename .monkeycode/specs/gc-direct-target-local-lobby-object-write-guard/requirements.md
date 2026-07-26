# Requirements: Direct Target Local Lobby Object Write Guard

## Scope

Protect the direct Local lobby write audit against reintroducing full object writes through a target GC pointer.

## Requirements

1. The audit shall reject production `.cpp` writes shaped like `options.client_target->GBE_local_lobby = ...`.
2. The audit shall keep existing direct object and direct field write diagnostics unchanged.
3. The change shall not alter production Dota GC behavior.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
