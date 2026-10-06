# gc-audit-any-token-helper design

## Current State

Wrapped hard-miss audit repeats `a in text or b in text` checks for prohibited route tokens in helper body and request path body.

## Design

- Add `contains_any_token(text, tokens)` beside the existing text matching helpers.
- Preserve substring membership semantics with `any(token in text for token in tokens)`.
- Replace wrapped hard-miss helper body and request body prohibited route checks.
- Add focused tests for present, absent, and empty token collections.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
