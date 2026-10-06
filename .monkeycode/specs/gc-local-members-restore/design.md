# gc-local-members-restore design

## Current State

Shared-to-local runtime restore compares Local and shared member vectors in the restore coordinator, then assigns `GBE_local_lobby.members` directly when they differ.

## Design

- Add `restore_lobby_members(GBE_LocalLobby &, const std::vector<GBE_DotaLobbyMemberState> &)`.
- Reuse `gbe::dota_lobby_flow::lobby_members_equal(...)` for comparison.
- Return `true` only when Local members are replaced.
- Replace only the runtime restore direct assignment.
- Keep changed aggregation and all post-restore side effects in the coordinator.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
