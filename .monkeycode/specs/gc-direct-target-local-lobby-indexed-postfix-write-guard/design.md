# Design: Direct Target Local Lobby Indexed Postfix Write Guard

## Scope

Add regression coverage for target-pointer indexed postfix Local lobby mutations.

## Approach

- Reuse audit 10d's target-aware indexed member write matcher, which already includes postfix `++` and `--` in the write operator set.
- Add a focused unit test for `options.client_target->GBE_local_lobby.members[0].team++`.
- Keep production `.cpp` files unchanged.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
