# Launch-State Side-Effect Executor Tasklist

This plan follows the completed launch-state push planner. The next goal is to make side-effect execution order explicit and testable before introducing any executor object or focused launch coordinator harness.

- [x] 1. Add a pure launch-state side-effect action sequence
  - Add a small enum in `gbe::dota_lobby_flow` that names the successful launch-state side effects in execution order.
  - Derive the sequence from `LaunchStatePushPlan` without touching coordinator state.
  - Keep production behavior unchanged in this step.

- [x] 2. Cover action order in focused flow tests
  - Assert successful plans produce cache subscription recording, cache-subscribed push, details-update push, rich-presence reapply, and last-game-state update in the existing production order.
  - Assert skipped plans produce an empty side-effect sequence.

- [x] 3. Checkpoint before production executor work
  - Ensure all tests pass, as follows:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

  - Result: `bash tools/run_gc_verification.sh --full` passed with payload helper tests `238/238`, handler smoke tests `43/43`, and audit issues `0`; `git diff --check` passed.

- [x] 4. Evaluate a tiny production executor
  - Continue only if the executor can consume built payloads and a plan without owning shared-state restore, capture, or payload building.
  - Stop if the executor needs broad `Steam_Game_Coordinator` construction or changes response order, rich presence order, or reason strings.
  - Decision: defer production executor. A tiny executor at this point would still be a coordinator member wrapper over `GBE_RecordDotaLobbyCacheSubscriptionState`, two `push_incoming_now` calls, `GBE_ReapplyDotaPracticeLobbyLaunchRichPresence`, and `GBE_SetLastDotaLaunchStatePushedGameState`, with no narrower test harness than the current planner/action-sequence coverage.

- [x] 5. Re-check focused launch coordinator harness
  - Reconsider a harness only after side-effect order is explicit and the executor context is small.
  - Decision: defer focused harness until payload building and side-effect execution can be represented without broad coordinator construction.

- [x] 6. Re-check focused launch coordinator harness after handler ordering coverage expansion
  - Goal: determine whether `GBE_PushDotaLaunchStateToClientPeer(...)` can be compiled into the existing handler smoke wrapper without broadening the harness.
  - Result: defer. The production file `gbe_dota_lobby_launch_coordinator.cpp` defines 15 launch/teardown/response members in one TU, including `GBE_TryQueueDotaPrelaunch021`, `GBE_SetDotaLobbyMemberRuntimeState`, `GBE_TryQueueDotaRuntimeLobbyDetailsUpdate`, `GBE_TryAdvanceDotaLaunchToRun`, `GBE_QueueDotaPostGameTeardown`, `GBE_SendDotaPracticeLobbyDetailsUpdate`, `GBE_PushDotaResponse`, and `GBE_SendDotaCustomGameLaunchSetupFlow`. The existing handler smoke stubs already provide several of these members, so including the whole TU would create definition conflicts and pull response/custom-game teardown linkage into a launch-state-only test.
  - Next safe seam: split the launch-state push member into a small production TU or helper only after payload building and side-effect execution have a narrow interface. Do not link the full launch coordinator TU into handler smoke tests.

- [x] 7. Extract captured-lobby plan-input mapping seam
  - Goal: make the captured lobby to `LaunchStatePushPlanInput` field mapping testable without touching payload building or side-effect execution.
  - Result: added `LaunchStateCapturedLobbyInput` plus `apply_captured_lobby_to_launch_state_push_plan_input(...)` in `gbe::dota_lobby_flow`, covered the mapping in `gbe_dota_lobby_flow_test`, and replaced the equivalent field assignments inside `GBE_PushDotaLaunchStateToClientPeer(...)`.
  - Stop condition: behavior remains equivalent; this seam only copies planner inputs and does not build payloads, push messages, reapply rich presence, or update last game state.

- [x] 8. Name launch-state payload build sequence
  - Goal: make payload build order explicit before introducing any production executor or focused coordinator harness.
  - Result: added `LaunchStatePayloadBuild` plus `launch_state_payload_builds(...)` in `gbe::dota_lobby_flow`, covered the successful order `CacheSubscribed` then `DetailsUpdate`, and covered skipped plans producing no payload build sequence.
  - Stop condition: production payload building remains in `GBE_PushDotaLaunchStateToClientPeer(...)`; this helper only describes build order and does not build or push messages.

- [x] 9. Name launch-state payload build requests
  - Goal: carry payload build order plus the `preserve_server_id` build flag as pure data before moving any production payload builder call.
  - Result: added `LaunchStatePayloadBuildRequest` plus `launch_state_payload_build_requests(...)` in `gbe::dota_lobby_flow`, covered successful request order, owner-LAN preserve propagation, non-preserve connect-endpoint behavior, and skipped plans producing no requests.
  - Stop condition: production payload building remains in `GBE_PushDotaLaunchStateToClientPeer(...)`; this helper only describes request metadata and does not build payloads, push messages, record cache subscriptions, reapply rich presence, or update last game state.

- [x] 10. Consume launch-state payload build requests in production
  - Goal: make `GBE_PushDotaLaunchStateToClientPeer(...)` follow the pure request sequence while keeping payload builder ownership and side-effect order unchanged.
  - Result: added the narrow member helper `GBE_BuildDotaLaunchStatePayload(...)`, which switches on `LaunchStatePayloadBuildRequest` and delegates to the existing authoritative `24` or `26` payload builders. The production launch-state push path now builds `response_24` and `response_26` from the tested request sequence.
  - Stop condition: no response push, cache subscription recording, rich-presence reapply, last-game-state update, failure log reason, or focused launch coordinator harness changed.

- [x] 11. Consume launch-state action sequence in production
  - Goal: make the production tail follow the pure action sequence after payloads are already built.
  - Result: added `GBE_ExecuteDotaLaunchStatePushActions(...)`, which consumes `launch_state_push_actions(plan)` and executes cache subscription recording, cache-subscribed push, details-update push, rich-presence reapply, and last-game-state update in the tested order.
  - Stop condition: the helper receives built payloads and a captured lobby snapshot; it does not restore shared state, select the target coordinator, capture lobby state, build payloads, change response metadata, or introduce a focused launch coordinator harness.

- [x] 12. Group launch-state payload builds behind a narrow helper
  - Goal: keep `GBE_PushDotaLaunchStateToClientPeer(...)` from indexing build requests directly while preserving the existing `24` and `26` failure log sites.
  - Result: added `GBE_BuildDotaLaunchStatePayloads(...)`, which consumes the tested request sequence, builds `response_24` then `response_26`, and reports the failed build kind to the caller.
  - Stop condition: the helper still delegates to the existing authoritative payload builders; it does not push responses, record cache subscriptions, reapply rich presence, update last game state, change failure log reason/text, or broaden harness linkage.

- [x] 13. Extract launch-state target plan-input mapping seam
  - Goal: make target/peer field mapping testable without moving target coordinator selection or settings reads.
  - Result: added `LaunchStateTargetInput` plus `apply_target_to_launch_state_push_plan_input(...)`, covered target field copying and planner gates in `gbe_dota_lobby_flow_test`, and replaced the equivalent field assignments inside `GBE_PushDotaLaunchStateToClientPeer(...)`.
  - Stop condition: this seam only copies planner inputs; it does not choose the target coordinator, restore shared state, capture lobby state, build payloads, push messages, or update last game state.

- [x] 14. Extract local-lobby captured input mapping seam
  - Goal: make the `GBE_LocalLobby` to `LaunchStateCapturedLobbyInput` conversion testable without moving lobby capture or settings reads.
  - Result: added `captured_lobby_input_from_local_lobby(...)`, covered copied launch fields and connect availability in `gbe_dota_lobby_flow_test`, and replaced the equivalent field construction inside `GBE_PushDotaLaunchStateToClientPeer(...)`.
  - Stop condition: the helper only converts fields; it does not capture lobby state, read last pushed game state, read target local steam id, build payloads, push messages, or update last game state.
