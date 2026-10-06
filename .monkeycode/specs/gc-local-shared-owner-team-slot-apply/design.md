# gc-local-shared-owner-team-slot-apply design

## Current State

7034 draft owner handling directly writes `GBE_local_lobby.owner_team` and `GBE_local_lobby.owner_slot`, then publishes updated lobby details when either field changes.

## Design

- Add `apply_lobby_owner_team(GBE_LocalLobby &, std::uint32_t)` and `apply_lobby_owner_slot(GBE_LocalLobby &, std::uint32_t)`.
- Return true only when the stored value changes.
- Reuse the helpers from `restore_lobby_owner_team(...)` and `restore_lobby_owner_slot(...)` to keep restore and local apply semantics aligned.
- Replace 7034 direct writes with helper calls while preserving the existing `draft_owner_slot != 0u` guard.
- Add focused lobby state tests for matching no-op and changed owner team/slot values.

## Validation

- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
