# gc-local-generation-apply design

## Current State

Multiple production paths directly assign `GBE_local_lobby.generation` after computing the current or next generation. Shared restore has a nearby generation restore helper with matching assignment semantics.

## Design

- Add `apply_lobby_generation(GBE_LocalLobby &, std::uint64_t)`.
- Make `restore_lobby_generation(...)` delegate to the apply helper.
- Replace direct generation writes in create, join, runtime reset, lifecycle runtime clear, and recover.
- Add focused tests for no-op and update behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
