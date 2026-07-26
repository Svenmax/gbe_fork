# gc-local-generic-lobby-id-apply design

## Current State

Generic lobby id is written directly in 7038 create, 7040 leave fallback, and leave-generic cleanup. Shared restore has a nearby `restore_lobby_generic_lobby_id(...)` helper with matching assignment semantics.

## Design

- Add `apply_lobby_generic_lobby_id(GBE_LocalLobby &, std::uint64_t)`.
- Make `restore_lobby_generic_lobby_id(...)` delegate to the apply helper.
- Replace only direct generic_lobby_id writes in create, lifecycle leave fallback, and leave-generic cleanup.
- Add focused tests for no-op, update, and zero clear behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
