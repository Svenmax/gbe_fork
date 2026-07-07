# GC Coordinator Boundaries

This document records the intended ownership boundaries after the Dota GC refactor. It is a maintenance guide, not a design proposal.

## Core Coordinator

`dll/steam_game_coordinator.cpp` owns the generic GC lifecycle and the top-level Dota dispatch entry points:

- GC initialization, shutdown, send/retrieve callbacks, and queue plumbing.
- `handle_dota_client_message` as the top-level Dota message dispatcher.
- Shared process-level state definitions that still need cross-TU accessors.
- Narrow state reset helpers, such as `clear_dota_runtime_state(...)` and `GBE_ClearDotaLobbyRuntimeState()`.

New Dota business logic should not be added here unless it is part of the GC lifecycle or top-level dispatch contract.

## Internal Shared Header

`dll/gbe_dota_gc_internal.h` is the temporary bridge between split translation units. It should expose declarations needed by multiple TUs, but it should not become a writeable global state bucket.

Preferred pattern:

- Expose const accessors or narrow mutation helpers instead of `extern` mutable state.
- Keep mutable definitions in one owning TU when practical.
- Add new declarations only when at least two production TUs need the symbol.

Current known debt:

- `GBE_shared_dota_lobby_state` is still directly shared across handlers, payload helpers, and state coordinators. Shrinking it needs a dedicated state facade or context object rather than a mechanical rename.

## Handler TUs

Handler files own request-specific decisions and preserve observable side-effect ordering:

- `gbe_dota_chat_handlers.cpp`: chat join/leave and chat-channel lifecycle requests.
- `gbe_dota_lobby_handlers.cpp`: lobby create/join/destroy/invite style requests.
- `gbe_dota_match_handlers.cpp`: match/runtime progression requests.
- `gbe_dota_inventory_handlers.cpp`: inventory/equip/unlock request handling.
- `gbe_dota_template_replay_handlers.cpp`: canned legacy template responses and item-use/open-treasure replay logic.
- `gbe_dota_post_login_handlers.cpp`: post-login synchronization and deferred startup flows.
- `gbe_dota_custom_game_handlers.cpp`: custom-game loading/readiness flow requests.
- `gbe_dota_misc_handlers.cpp`: one-off low-coupling Dota requests.

Handlers may call coordinator side-effect seams such as `GBE_PushDotaResponse(...)`, `GBE_PublishSharedDotaLobbyState(...)`, or reset helpers. They should not add new direct writes to shared global state when a narrow helper can express the intent.

## State Coordinators

State coordinator files own transitions and derived state publication:

- `gbe_dota_lobby_state_coordinator.cpp`: publishing local lobby state to shared state and reacting to member/lobby state changes.
- `gbe_dota_lobby_flow_coordinator.cpp`: signout, postgame, cache unsubscribe, and lifecycle finalization flows.
- `gbe_dota_lobby_launch_coordinator.cpp`: launch phase progression and launch-state push behavior.
- `gbe_dota_lobby_snapshot_coordinator.cpp`: snapshot/cache-subscribed payload construction from current lobby state.
- `gbe_dota_welcome_coordinator.cpp`: welcome, inventory, and server-hello follow-up behavior.

New teardown or reset behavior should go through the narrow reset helpers first. If a path intentionally resets only part of runtime state, document that near the call site.

## Payload And Wire Helpers

Payload and wire helper files should stay mostly pure:

- `gbe_proto_wire.cpp` and `gbe_dota_gc_wire.cpp`: raw protobuf-like parsing/building helpers.
- `gbe_gc_message_utils.cpp`: shared GC message wrapper helpers.
- `gbe_dota_payload_*_helpers.cpp`: payload adaptation, template patching, logging, and item/lobby/wire transformations.
- `gbe_dota_lobby_state.cpp`, `gbe_dota_lobby_flow.cpp`, `gbe_dota_custom_game.cpp`: smaller domain helpers with offline test coverage.

These files should not gain coordinator pointers, queue mutation, network/file handles, or broad access to mutable shared state. When state is needed, pass it as a const input or use a narrow read-only accessor.

## Tests And Audit

`tools/run_gc_verification.sh` is the main local verification entry point:

- `--fast` runs the PR-friendly smoke suite plus audit/style checks.
- `--full` runs the complete offline GC suite plus audit/style checks.

`tools/_audit_gc_refactor.py` enforces structural guardrails, including source-list inclusion, handler side-effect seams, template ownership, and high-risk reason inventory coverage. Reason string governance lives in `docs/gc/reason-trace-governance.md`.
