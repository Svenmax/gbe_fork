# GC Direct Target Local Lobby Parenthesized Indexed Member Write Guard Design

## Context

Audit 10d rejects parenthesized indexed member Local lobby writes and target indexed member Local lobby writes. The target root can also be wrapped in parentheses before indexed member assignment.

## Design

- Add one regression test covering `(options.client_target->GBE_local_lobby).members[0].team = team`.
- Reuse the existing parenthesized indexed member matcher, which supports the optional target prefix.
- Keep production code unchanged.

## Verification

- Run `git diff --check`.
- Run `bash tools/run_gc_verification.sh --full`.

## Risk

This is a test/documentation guard slice over existing audit behavior.
