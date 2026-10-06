# gc-audit-adapter-handler-call-helper design

## Current State

Post-login dispatch audit extracts adapter request-handler calls inline with a regex in the registry entry loop.

## Design

- Add `extract_adapter_handler_calls(body)` near the existing audit parsing helpers.
- Keep the exact existing regex shape and return order.
- Replace the inline `re.findall(...)` in `audit_post_login_dispatch(...)`.
- Add focused helper tests for one call, multiple calls, non-matching function names, and missing calls.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
