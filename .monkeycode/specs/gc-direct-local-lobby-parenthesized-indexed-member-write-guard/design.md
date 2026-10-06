# GC Direct Local Lobby Parenthesized Indexed Member Write Guard Design

## Context

Audit 10d rejects `GBE_local_lobby.members[0].team = team` and `(GBE_local_lobby).members[0] = member`. The indexed member variant with a parenthesized root should be covered by the same direct field write guard.

## Design

- Add a parenthesized indexed member write matcher for `(GBE_local_lobby).<field>[index].<field> <op>`.
- Add one regression test covering `(GBE_local_lobby).members[0].team = team`.
- Reuse the existing direct field write failure message.

## Verification

- Run `git diff --check`.
- Run `bash tools/run_gc_verification.sh --full`.

## Risk

The matcher remains scoped to production `.cpp` source and only affects audit reporting.
