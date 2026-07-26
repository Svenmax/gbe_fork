# Requirements: GC Local Assignment Comment Truth

## Scope

Update stale comments and Local lobby inventory text after all real `GBE_local_lobby = ...` object writes were moved behind named state helpers.

## Requirements

- Comments shall reference the current helper boundaries instead of direct Local object writes.
- `LOCAL_LOBBY_USAGE.md` shall describe complete Local snapshot writes as helper-owned boundaries.
- The change shall not modify production behavior, routing inventory, or shared Store behavior.
