# gc-audit-marker-order-helper design

## Current State

Routing audits compare marker positions with repeated `body.find(a) > body.find(b)` checks after confirming both markers are present.

## Design

- Add `marker_appears_after(text, marker, reference_marker)` beside the existing text marker helpers.
- Preserve current missing-marker behavior by returning false if either marker is absent.
- Replace direct conditional fallback and wrapped hard-miss order comparisons.
- Add focused tests for after, before, missing marker, and missing reference behavior.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
