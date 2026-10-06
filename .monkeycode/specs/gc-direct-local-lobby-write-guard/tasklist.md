# Task List: GC Direct Local Lobby Write Guard

## Implementation

- [x] Add direct Local lobby write audit helper.
- [x] Wire the audit into CLI output, summary, and failure exit.
- [x] Remove stale handler responsibility baseline allowances for direct Local assignments.
- [x] Add regression tests for accepted helper usage and rejected direct writes.

## Verification

- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Full GC verification and diff check pass.
- [x] Direct Local lobby object/field writes are guarded by audit.
- [x] Production behavior is unchanged.
