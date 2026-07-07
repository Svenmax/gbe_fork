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
