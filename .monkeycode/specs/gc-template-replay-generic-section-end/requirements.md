# gc-template-replay-generic-section-end requirements

Status: Completed

## Problem

The template replay inventory audits stop §4 at the next `---` separator. If that separator is removed while the next section heading remains, rows outside §4 can be parsed as template-only or registry-defensive inventory rows.

## Requirements

- The template-only audit shall stop parsing §4 at the next level-2 heading after `## 4. template_replay`, regardless of `---` separator presence.
- The registry-defensive template audit shall use the same §4 boundary behavior.
- Existing missing section, emsg set, duplicate, and helper checks shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- Regression tests shall prove that rows after §4 are ignored when the separator after §4 is missing.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
