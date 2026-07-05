# GC Reason and Trace Governance

## Scope

This note records the high-risk Dota GC reason strings that are part of the observable refactor contract. These strings are used by handler tests, action recorders, lobby publish hooks, details updates, and diagnostic logs to verify side-effect intent and ordering.

## High-Risk Reason Inventory

| Area | Reason | Keep or Rename | Coverage |
| --- | --- | --- | --- |
| Inventory equip | `equip_forward_host_resubscribe_server` | Keep | Handler smoke test asserts server GC cache push reason |
| Inventory equip | `equip_items_refresh` | Keep | Planner focused assertions cover snapshot refresh intent |
| Chat leave | `7272_7014` | Keep | Handler smoke test asserts postgame 7014 response reason |
| Chat leave | `7272_leave_chat` | Keep | Handler smoke test asserts publish reason |
| Lobby abandon | `7035_current_game_disconnect` | Keep | Code path uses it for abandoned-lobby suppression; current direct `push_incoming_now` response has no action reason metadata |
| Lobby abandon | `postgame_teardown_7014` | Keep | Handler smoke test asserts postgame teardown response reason |
| Match 7034 | `7034_connected_player` | Keep | Handler smoke test asserts connected-player publish reason |
| Match 7034 | `7034_disconnected_player` | Keep | Handler smoke test asserts disconnected-player publish reason |
| Match 7034 | `runtime AP hero_selection fallback strategy_time` | Keep | Handler smoke test asserts runtime update payload carries strategy-time reason |
| Match 7034 | `7034_launch_poll` | Keep | Handler smoke test asserts details update reason |
| Match 7070 | `7070_custom_game_ready_up_run_ack` | Keep | Handler smoke test asserts ready-up publish reason |
| Match 8052 | `8052_started_loading` | Keep | Handler smoke test asserts started-loading publish reason |
| Match 8053 | `8053_finished_loading` | Keep | Handler smoke test asserts successful finished-loading publish reason |
| Match 8053 | `8053_load_failed` | Keep | Handler smoke test asserts failed finished-loading publish and details update reasons when result text indicates disconnect |

## Naming Rule

New reason strings should use `emsg_or_flow_event` style:

- Begin with the triggering emsg when the reason is tied to one request, such as `8052_started_loading`.
- Use the flow name when the reason describes a cross-message lifecycle event, such as `postgame_teardown_7014`.
- Express the business trigger or protocol event, not the temporary implementation structure.
- Keep reason values stable across refactors unless tests and this inventory are updated in the same change.

## Trace Boundary

- `GBE_GC_DebugLog` owns contextual diagnostics for branch decisions, failures, and runtime observations.
- `GBE_LogDotaResponsePacket` owns outbound response observation: reason, emsg, wrapped state, payload size, and lobby state summary.
- Proto boundary trace owns raw wire parse/build edges and should stay near wire/helper code.
- Reason strings on recorded actions and publish hooks remain the stable semantic labels used by tests.
