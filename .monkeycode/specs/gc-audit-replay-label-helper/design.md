# gc-audit-replay-label-helper design

## Current State

Post-login dispatch audit builds replay fixture labels inline by splitting each fixture line with `split(maxsplit=2)` and collecting the third field when present.

## Design

- Add `extract_replay_fixture_labels(fixture_text)` near existing audit parsing helpers.
- Preserve existing set output and malformed-line ignore behavior.
- Replace the inline fixture label loop in `audit_post_login_dispatch(...)`.
- Add focused helper tests for valid lines, malformed lines, blank lines, and labels containing spaces.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
