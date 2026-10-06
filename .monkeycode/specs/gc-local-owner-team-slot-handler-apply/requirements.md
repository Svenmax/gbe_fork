# gc-local-owner-team-slot-handler-apply requirements

## Scope

Reduce Local lobby write scatter by making 7047 set team slot owner updates reuse the existing owner team/slot apply helpers.

## Requirements

- 7047 set team slot shall apply owner team through `apply_lobby_owner_team(...)` when the local user owns the lobby and the request has team.
- 7047 set team slot shall apply owner slot through `apply_lobby_owner_slot(...)` when the local user owns the lobby and the request has slot.
- Existing member team/slot update, bot difficulty update, normalization, publish, details update, and ack ordering shall remain unchanged.

## Non-Goals

- Do not change bot difficulty ownership.
- Do not change member slot update behavior.
- Do not change 7047 response payload behavior.
