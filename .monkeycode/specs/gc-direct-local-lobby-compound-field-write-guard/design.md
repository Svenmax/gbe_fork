# Design: Direct Local Lobby Compound Field Write Guard

## Approach

Extend the audit 10d direct field write operator matcher from simple `=` to direct write operators:

- `=`
- compound assignments such as `+=`
- increment and decrement operators

## Behavior

- The audit continues to report all matched forms as direct `GBE_local_lobby` field writes.
- Production code remains unchanged.

## Risk

Low. The change only broadens audit detection for direct Local lobby field mutation syntax.
