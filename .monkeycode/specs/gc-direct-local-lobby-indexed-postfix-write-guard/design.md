# Design: Direct Local Lobby Indexed Postfix Write Guard

## Scope

Add regression coverage for direct indexed postfix Local lobby mutations.

## Approach

- Reuse audit 10d's indexed field/member write matchers, which already include postfix `++` and `--` in the write operator set.
- Add a focused unit test for `GBE_local_lobby.members[0].team++`.
- Keep production `.cpp` files unchanged.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
