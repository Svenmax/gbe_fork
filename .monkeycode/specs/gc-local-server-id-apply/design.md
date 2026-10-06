# gc-local-server-id-apply design

## Current State

Recover coordinator directly assigns `GBE_local_lobby.server_id = derived_server_id` after deriving a server id and checking existing Local server_id. Runtime metadata helper also owns exact server_id application for generic metadata publish.

## Design

- Add `apply_lobby_server_id(GBE_LocalLobby &, std::uint64_t)`.
- Make `apply_runtime_metadata(...)` reuse the server_id helper.
- Replace only the recover coordinator direct server_id assignment.
- Add focused tests for no-op, update, and zero clear behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
