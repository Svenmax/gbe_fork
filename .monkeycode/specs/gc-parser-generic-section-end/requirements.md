# gc-parser-generic-section-end requirements

Status: Completed

## Problem

The legacy wrapped parser audit stops §5 at the next `---` separator. If that separator is removed while the next section heading remains, rows outside §5 can satisfy the LEGACY_UNUSED marker check.

## Requirements

- The legacy wrapped parser audit shall stop parsing §5 at the next level-2 heading after `## 5. 解析层职责`, regardless of `---` separator presence.
- Existing missing section, LEGACY_UNUSED marker, and production call-site checks shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- Regression tests shall prove that rows after §5 are ignored when the separator after §5 is missing.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
