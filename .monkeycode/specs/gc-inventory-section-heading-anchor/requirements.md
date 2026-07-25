# gc-inventory-section-heading-anchor requirements

Status: Completed

## Problem

The shared inventory section helper located headings with a plain substring search. A prose line that mentions a heading token before the actual Markdown heading can become the section start.

## Requirements

- `inventory_section(...)` shall only treat a heading token as a section start when it appears at the beginning of a line.
- Existing next level-2 heading section-end behavior shall remain unchanged.
- Existing missing-heading behavior shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Helper-level tests shall cover inline heading text before the real heading.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
