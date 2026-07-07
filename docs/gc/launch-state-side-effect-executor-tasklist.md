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

- [ ] 4. Evaluate a tiny production executor
  - Continue only if the executor can consume built payloads and a plan without owning shared-state restore, capture, or payload building.
  - Stop if the executor needs broad `Steam_Game_Coordinator` construction or changes response order, rich presence order, or reason strings.

- [ ] 5. Re-check focused launch coordinator harness
  - Reconsider a harness only after side-effect order is explicit and the executor context is small.
