# gc-local-clear-helper requirements

## Scope

Reduce Local object replacement scatter by moving pure Local lobby clear assignments behind a named state helper.

## Requirements

- Pure Local lobby clear operations shall use `gbe::dota_lobby_state::clear_local_lobby(...)`.
- Existing generation capture, shared Store clear, generic lobby leave, launch-state clear, reconnect context clear, and diagnostics order shall remain unchanged.
- Plan apply assignments shall remain out of scope.

## Non-Goals

- Do not change create/join/launch plan application.
- Do not change shared Store clear behavior.
- Do not change generation advance behavior.
