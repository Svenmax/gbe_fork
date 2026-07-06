# Dota GC Maintenance Docs

These documents preserve the long-term maintenance guidance extracted from the GC refactor work, without keeping agent-private process directories in the product repository.

- `coordinator-boundaries.md`: ownership boundaries for coordinator, handler, state, payload, and wire files.
- `verification-and-build.md`: local/CI verification gates and build entrypoint rules.
- `internal-header-shrink-plan.md`: plan for shrinking `gbe_dota_gc_internal.h` safely.
- `dependency-seams.md`: side-effect seams, dependency touchpoints, and error-boundary rules.
- `future-refactor-plan.md`: follow-up refactor sequence, risk gates, and stop conditions.
- `follow-up-task-list.md`: executable checklist for follow-up tests, seams, facades, and verification.
- `shared-lobby-state-contract.md`: current contract and safe facade direction for `GBE_shared_dota_lobby_state`.
- `lifecycle-state-map.md`: lobby lifecycle paths mapped to state, side effects, risks, and tests.
- `test-coverage-map.md`: offline GC tests mapped to the behavior they protect.
- `reason-trace-governance.md`: high-risk reason string inventory used by `tools/_audit_gc_refactor.py`.
