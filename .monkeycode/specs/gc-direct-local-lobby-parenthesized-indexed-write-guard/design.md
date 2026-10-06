# GC Direct Local Lobby Parenthesized Indexed Write Guard Design

## Context

Audit 10d rejects direct indexed writes such as `GBE_local_lobby.members[0] = member`. Parentheses around the root Local lobby object create a syntactic variant that should be guarded by the same audit category.

## Design

- Add a parenthesized indexed field write matcher for `(GBE_local_lobby).<field>[index] <op>`.
- Add one regression test covering `(GBE_local_lobby).members[0] = member`.
- Reuse the existing direct field write failure message.

## Verification

- Run `git diff --check`.
- Run `bash tools/run_gc_verification.sh --full`.

## Risk

The matcher remains scoped to production `.cpp` source and only affects audit reporting.
