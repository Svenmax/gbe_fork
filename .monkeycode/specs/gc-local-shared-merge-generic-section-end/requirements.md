# gc-local-shared-merge-generic-section-end requirements

Status: Completed

## Problem

`audit_local_shared_merge_inventory()` stops the Local/shared merge inventory section only at a hard-coded `## 6.` heading. If following sections are renumbered, rows outside §5 can be parsed as merge inventory rows.

## Requirements

- The audit shall stop parsing Local/shared merge inventory at the next level-2 heading after `## 5. Local/shared merge inventory`, regardless of heading number.
- Existing missing section, entrypoint, owner, field group, duplicate, and definition checks shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- A regression test shall prove that rows after a renumbered next heading are ignored.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
