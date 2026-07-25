# gc-audit-function-call-helper design

## Current State

Several ownership audits repeat `re.search(r"\b" + re.escape(symbol) + r"\s*\(", text)` to detect function calls.

## Design

- Add `contains_function_call(text, symbol)` next to shared audit helpers.
- Preserve the existing word-boundary + escaped-symbol + optional-whitespace-before-parenthesis semantics.
- Replace same-shape lifecycle and reconnect ownership checks.
- Add focused helper tests for positive, missing, escaped-symbol, and word-boundary behavior.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
