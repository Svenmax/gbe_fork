# gc-fallback-generic-section-end requirements

Status: Completed

## Problem

The fallback inventory audits stop §2 at the next `---` separator. If that separator is removed while the next section heading remains, rows outside §2 can be parsed as direct conditional fallback or wrapped hard miss inventory rows.

## Requirements

- The direct conditional fallback audit shall stop parsing §2 at the next level-2 heading after `## 2. 仍留在 if/fallback 的路径`, regardless of `---` separator presence.
- The wrapped hard miss audit shall use the same §2 boundary behavior.
- Existing missing section, emsg set, duplicate, helper order, and hard miss marker checks shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- Regression tests shall prove that rows after §2 are ignored when the separator after §2 is missing.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
