# gc-audit-request-body-marker-slicing design

## Current State

`audit_direct_conditional_fallback_routing(...)` and `audit_wrapped_hard_miss_routing(...)` still hand-roll `find(...)` pairs to slice request bodies.

## Design

- Replace the direct request body slicing with `text_between_markers(handler_text, direct_start_marker, add_socket_marker)`.
- Replace the wrapped request body slicing with `text_between_markers(handler_text, wrapped_start_marker, find_top_source_tv_marker)`.
- Reuse existing `TextBetweenMarkersHelperTest` coverage because the helper contract is unchanged.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
