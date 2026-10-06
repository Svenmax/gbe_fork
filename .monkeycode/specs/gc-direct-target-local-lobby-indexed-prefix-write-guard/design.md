# Design: Direct Target Local Lobby Indexed Prefix Write Guard

## Scope

Add regression coverage for target-pointer indexed prefix Local lobby mutations.

## Approach

- Reuse audit 10d's target-aware `direct_prefix_indexed_write` matcher.
- Add a focused unit test for `++options.client_target->GBE_local_lobby.members[0].team`.
- Keep production `.cpp` files unchanged.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
