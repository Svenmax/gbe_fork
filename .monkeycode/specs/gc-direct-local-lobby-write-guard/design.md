# Design: GC Direct Local Lobby Write Guard

## Approach

Add `audit_direct_local_lobby_writes(...)` to `tools/_audit_gc_refactor.py`. It scans stripped production `.cpp` source text for direct object and field writes to `GBE_local_lobby` and reports file-level counts.

## Integration

- Print the guard as `AUDIT 10d: Direct Local lobby writes` after Local/shared merge inventory.
- Include issue count in the summary.
- Include failures in the final exit condition.
- Remove stale handler responsibility baseline allowances for `GBE_local_lobby assignment`.

## Tests

- Accept helper-wrapped calls and comments.
- Reject direct object writes.
- Reject direct field writes.
- Reject direct target-pointer field writes.
