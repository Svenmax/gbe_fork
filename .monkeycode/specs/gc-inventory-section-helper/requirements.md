# gc-inventory-section-helper requirements

Status: Completed

## Problem

Inventory audit functions duplicate the same level-2 section slicing logic. Repeated offset arithmetic makes future section boundary edits easier to drift.

## Requirements

- Audit code shall use one shared helper to locate an inventory section by heading and stop at the next level-2 heading.
- Missing section behavior shall remain unchanged for each audit consumer.
- Existing registry, template replay, fallback, parser, and Local/shared merge checks shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Helper-level tests shall cover the next-heading boundary and missing-heading result.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
