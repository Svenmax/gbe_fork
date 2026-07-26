# Design: Direct Target Local Lobby Indexed Mutating Method Guard

## Scope

Add regression coverage for target-pointer indexed mutating method calls on Local lobby member chains.

## Approach

- Reuse audit 10d's target-aware indexed mutating method matcher.
- Add a focused unit test for `options.client_target->GBE_local_lobby.members[0].slots.clear()`.
- Keep production `.cpp` files unchanged.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
