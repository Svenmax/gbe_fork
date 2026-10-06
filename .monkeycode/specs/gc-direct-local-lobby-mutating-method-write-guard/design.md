# Design: Direct Local Lobby Mutating Method Write Guard

## Approach

Add a focused audit 10d matcher for direct `GBE_local_lobby` field chains followed by common mutating methods.

## Mutating Methods

- `push_back`
- `emplace_back`
- `clear`
- `erase`
- `insert`
- `assign`
- `resize`
- `swap`

## Behavior

- Matched method calls reuse the direct `GBE_local_lobby` field write diagnostic.
- Production code remains unchanged.

## Risk

Low. The method list is explicit and scoped to direct `GBE_local_lobby` field chains.
