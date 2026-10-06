# gc-inventory-section-exact-heading requirements

Status: Completed

## Problem

The shared inventory section helper anchors heading lookup at line start, but a longer heading with the same prefix can still satisfy the lookup before the intended heading.

## Requirements

- `inventory_section(...)` shall only treat a heading as matching when the requested heading text is followed by line end or a parenthetical annotation.
- Existing next level-2 heading section-end behavior shall remain unchanged.
- Existing missing-heading behavior shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Helper-level tests shall cover a longer same-prefix heading before the real heading and a parenthetical heading annotation.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
