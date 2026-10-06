# gc-local-runtime-metadata-apply design

## Current State

Generic metadata publish writes `GBE_local_lobby.connect` and `GBE_local_lobby.server_id` directly after generic lobby metadata fields and before the shared Store compare_update.

## Design

- Add `apply_runtime_metadata(GBE_LocalLobby &, const std::string &, std::uint64_t)`.
- Preserve metadata publish semantics by applying empty connect and zero server id values exactly.
- Keep `apply_runtime_connect(...)` independent because 4508 empty connect preserves existing Local connect.
- Replace only the two direct Local writes in the publish coordinator.
- Add focused tests for empty connect application, changed values, matching no-op, and zero server id application.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
