# gc-audit-word-token-helper design

## Current State

Several audit functions repeat `re.search(r"\b" + re.escape(token) + r"\b", text)` to detect whole-word retired tokens.

## Design

- Add `contains_word_token(text, token)` beside `contains_function_call(...)`.
- Preserve escaped-token and word-boundary semantics exactly.
- Replace same-shape checks in retired lifecycle, reconnect, and shared-lobby compatibility audits.
- Add focused helper tests for whole-word match, longer-word rejection, and escaped token text.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
