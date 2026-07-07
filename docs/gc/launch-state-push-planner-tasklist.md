# Launch-State Push Planner Tasklist

This plan continues the GC maintenance refactor by turning the remaining broad `GBE_PushDotaLaunchStateToClientPeer(...)` decision path into a small, testable planner. The goal is maintenance safety: make launch-state push skip/push decisions reviewable without broadening the handler smoke harness or starting Dota sub-object extraction.

- [x] 1. Add a pure launch-state push planner
  - Add small input/output structs to `gbe::dota_lobby_flow` for launch-state client-peer push decisions.
  - Keep inputs scalar and explicit; do not pass `Steam_Game_Coordinator *` or mutable lobby state.
  - Model existing decisions only: target eligibility, shared suppression, captured lobby availability, launch-state eligibility, duplicate game-state suppression, owner-LAN preserve eligibility, and push action booleans.

- [x] 2. Cover planner decisions with focused flow tests
  - Add tests in `gbe_dota_lobby_flow_test` for missing/invalid target, suppressed shared lobby, inactive captured lobby, ineligible launch state, duplicate game state, normal push, and owner-LAN preserve.
  - Keep tests pure and independent from handler smoke stubs.

- [x] 3. Checkpoint before production integration
  - Ensure all tests pass, as follows:

```bash
bash tools/run_gc_verification.sh --full
git diff --check
```

  - Result: `bash tools/run_gc_verification.sh --full` passed with payload helper tests `238/238`, handler smoke tests `43/43`, and audit issues `0`; `git diff --check` passed.

- [ ] 4. Integrate planner into `GBE_PushDotaLaunchStateToClientPeer(...)`
  - Build planner input from existing coordinator state.
  - Use planner output to drive the existing restore/capture/build/push/rich-presence/last-state sequence.
  - Preserve existing reason strings, response order, wrapped `26` behavior, cache subscription recording, rich presence ordering, and last-game-state update.

- [ ] 5. Re-evaluate focused launch coordinator harness
  - Add a focused harness only if planner integration leaves a small explicit context for side-effect assertions.
  - Stop if the harness requires broad coordinator construction, protobuf expansion, or handler smoke wrapper linkage expansion.

- [ ] 6. Re-check Dota sub-object extraction readiness
  - Keep extraction deferred unless the launch planner, ownership seams, side-effect ordering, and shared-state mutation boundaries form a small coherent dependency surface.
