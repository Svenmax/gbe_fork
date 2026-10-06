# gc-local-chat-channel-apply requirements

## Scope

Reduce Local lobby write scatter by moving 7009 join and 7272 leave chat channel field writes behind named state helpers.

## Requirements

- 7009 join shall apply `has_chat_channel`, `chat_channel_id`, `chat_channel_name`, and `chat_channel_type` through a `gbe::dota_lobby_state` helper.
- 7272 leave shall clear the chat channel field group through a `gbe::dota_lobby_state` helper.
- Existing generated channel id reuse behavior shall remain unchanged.
- Existing host sync, response push, publish, and tombstone ordering shall remain unchanged.

## Non-Goals

- Do not change chat payload formats.
- Do not change broadcast channel handling.
- Do not move generic metadata capture or shared Store ownership.
