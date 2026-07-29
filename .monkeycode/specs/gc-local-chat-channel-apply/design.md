# gc-local-chat-channel-apply design

## Current State

7009 join directly writes the Local chat channel field group before generic metadata capture. 7272 leave directly clears the same field group before later publish or teardown behavior.

## Design

- Add `apply_chat_channel(GBE_LocalLobby &, std::uint64_t, const std::string &, std::uint32_t)`.
- Add `clear_chat_channel(GBE_LocalLobby &)`.
- Keep 7009 channel id generation in the handler so RNG side effects remain under the existing guard.
- Replace only the chat channel field writes in 7009 and 7272.
- Add focused lobby state tests for apply, no-op apply, clear, and no-op clear behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
