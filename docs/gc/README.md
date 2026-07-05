# Dota GC Maintenance Docs

These documents preserve the long-term maintenance guidance extracted from the GC refactor work, without keeping agent-private process directories in the product repository.

- `coordinator-boundaries.md`: ownership boundaries for coordinator, handler, state, payload, and wire files.
- `verification-and-build.md`: local/CI verification gates and build entrypoint rules.
- `internal-header-shrink-plan.md`: plan for shrinking `gbe_dota_gc_internal.h` safely.
- `dependency-seams.md`: side-effect seams, dependency touchpoints, and error-boundary rules.
- `reason-trace-governance.md`: high-risk reason string inventory used by `tools/_audit_gc_refactor.py`.
