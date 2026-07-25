# gc-registry-generic-section-end requirements

Status: Completed

## Problem

`audit_registry_inventory_guard()` stops the registry inventory section at the next `---` separator. If that separator is removed while the next section heading remains, §2 numeric rows can be parsed as registry rows.

## Requirements

- The audit shall stop parsing registry inventory at the next level-2 heading after `## 1. Registry`, regardless of `---` separator presence.
- Existing missing section, emsg set, metadata, and duplicate row checks shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that §2 rows are ignored when the separator after §1 is missing.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
