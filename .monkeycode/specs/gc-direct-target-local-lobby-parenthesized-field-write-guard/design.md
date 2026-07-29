# GC Direct Target Local Lobby Parenthesized Field Write Guard Design

## Context

Audit 10d already rejects direct target writes such as `options.client_target->GBE_local_lobby.state = ...` and parenthesized local writes such as `(GBE_local_lobby).state = ...`. A target pointer can also be wrapped in parentheses before field assignment.

## Design

- Extend the parenthesized field write matcher to include the same optional target prefix used by non-parenthesized direct writes.
- Add one regression test covering `(options.client_target->GBE_local_lobby).state = 1u`.
- Keep the existing audit failure message and count category.

## Verification

- Run `git diff --check`.
- Run `bash tools/run_gc_verification.sh --full`.

## Risk

The matcher remains scoped to production `.cpp` source passed to audit 10d and only reports direct Local lobby field writes.
