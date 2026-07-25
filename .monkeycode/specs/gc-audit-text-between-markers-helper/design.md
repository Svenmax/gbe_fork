# gc-audit-text-between-markers-helper design

## Current State

Several routing audits manually compute `start = text.find(start_marker)`, `end = text.find(end_marker)`, then return `text[start:end]` only when both markers exist.

## Design

- Add `text_between_markers(text, start_marker, end_marker)` near the existing audit helpers.
- Preserve existing behavior by returning `""` if either marker is absent.
- Replace helper body slicing in routing audit checks where the current code already uses identical marker semantics.
- Add focused tests for successful slicing and missing marker behavior.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
