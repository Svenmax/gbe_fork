# gc-legacy-wrapped-parser-section-guard requirements

Status: Completed

## Problem

`audit_legacy_wrapped_parser_guard()` currently accepts a `GBE_ExtractWrappedDotaDirectContext` + `LEGACY_UNUSED` marker anywhere in `MESSAGE_ROUTING_INVENTORY.md`, so unrelated text outside the parser responsibility table can satisfy the contract.

## Requirements

- The audit shall require `GBE_ExtractWrappedDotaDirectContext` to be documented as `LEGACY_UNUSED` in `MESSAGE_ROUTING_INVENTORY.md` section `## 5. 解析层职责`.
- The production no-call-site check shall remain unchanged.
- Production C++ routing behavior shall remain unchanged.
- A regression test shall prove that moving the marker outside §5 fails the audit.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
