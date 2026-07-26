# Design: Direct Local Lobby Indexed Mutating Method Guard

## Scope

Extend audit 10d to cover direct indexed mutating method calls on Local lobby member chains.

## Approach

- Add a matcher for `GBE_local_lobby.<field>[index].<field>.<mutating_method>(...)`.
- Count matches as direct Local lobby field writes.
- Add a focused unit test for `GBE_local_lobby.members[0].slots.clear()`.
- Keep production `.cpp` files unchanged.

## Verification

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
