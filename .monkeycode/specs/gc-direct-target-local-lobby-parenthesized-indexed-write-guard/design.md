# GC Direct Target Local Lobby Parenthesized Indexed Write Guard Design

## Context

Audit 10d rejects parenthesized indexed Local lobby writes and target indexed Local lobby writes. The target object can be wrapped in parentheses before indexed assignment, so that shape needs explicit regression coverage.

## Design

- Add one regression test covering `(options.client_target->GBE_local_lobby).members[0] = member`.
- Reuse the existing parenthesized indexed matcher, which already supports the optional target prefix.
- Keep production code unchanged.

## Verification

- Run `git diff --check`.
- Run `bash tools/run_gc_verification.sh --full`.

## Risk

This is a test/documentation guard slice over existing audit behavior.
