# gc-audit-emsg-token-resolver design

## Current State

Registry inventory audit resolves each production registry emsg token inline with `re.fullmatch(...)`, constants lookup, and an unknown-token diagnostic branch.

## Design

- Add `resolve_emsg_token(token, constants)` near existing audit parsing helpers.
- Return the suffix-stripped numeric string for `1234` and `1234u`.
- Return `constants[token]` for known named protocol constants.
- Return `None` for unknown named tokens so the caller keeps the existing diagnostic string.
- Add focused helper tests for numeric, unsigned numeric, named, and unknown token behavior.

## Validation

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
