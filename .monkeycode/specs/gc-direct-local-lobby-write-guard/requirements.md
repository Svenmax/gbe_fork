# Requirements: GC Direct Local Lobby Write Guard

## Scope

Add an audit guard that prevents production `.cpp` files from reintroducing direct `GBE_local_lobby` object or field writes after those writes were moved behind named state helpers.

## Requirements

- The audit shall ignore comments.
- The audit shall reject direct `GBE_local_lobby = ...` writes.
- The audit shall reject direct `GBE_local_lobby.<field> = ...` writes, including target-pointer forms such as `client->GBE_local_lobby.<field> = ...`.
- Helper internals that write through a `GBE_LocalLobby &lobby` parameter shall remain allowed.
- The full GC verification gate shall include the guard.
