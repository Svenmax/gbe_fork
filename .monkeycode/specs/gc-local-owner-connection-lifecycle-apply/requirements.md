# gc-local-owner-connection-lifecycle-apply requirements

## Scope

Reduce Local lobby write scatter by making owner connect/disconnect lifecycle paths reuse the existing owner connected apply helper.

## Requirements

- `on_client_connected(...)` shall apply owner connected state through `apply_lobby_owner_connected(...)`.
- `on_client_disconnected(...)` shall apply owner disconnected state through `apply_lobby_owner_connected(...)`.
- Existing abandoned-lobby suppress guard shall remain unchanged.
- Existing PostGame publish suppression and owner inventory cache preservation behavior shall remain unchanged.

## Non-Goals

- Do not change member connected handling.
- Do not change shared restore behavior.
- Do not change reconnect or inventory unsubscribe behavior.
