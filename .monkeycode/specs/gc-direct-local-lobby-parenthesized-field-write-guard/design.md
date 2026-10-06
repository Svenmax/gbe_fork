# Design: Direct Local Lobby Parenthesized Field Write Guard

## Scope

Extend audit 10d to cover parenthesized direct Local lobby field writes.

## Approach

- Add a matcher for `(GBE_local_lobby).<field> <write-op>`.
- Count matches as direct Local lobby field writes.
- Add a focused unit test for `(GBE_local_lobby).state = 1u`.
- Keep production `.cpp` files unchanged.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
