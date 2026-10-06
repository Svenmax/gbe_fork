# gc-inventory-markdown-cells-helper requirements

Status: Completed

## Problem

Inventory audits repeat the same Markdown table row cell splitting expression. This makes future table parsing hardening prone to drift across routing and Local/shared merge guards.

## Requirements

- Inventory audits shall use one shared helper for basic Markdown table row cell splitting.
- The helper shall preserve current behavior for leading/trailing pipes and per-cell whitespace trimming.
- Routing and Local/shared merge inventory audit behavior shall remain unchanged.
- Production C++ behavior shall remain unchanged.
- Helper-level tests shall cover leading/trailing pipes and whitespace trimming.

## Verification

- `python3 tools/test_audit_gc_refactor.py`
- `python3 tools/_audit_gc_refactor.py`
- `bash tools/run_gc_verification.sh --full`
- `git diff --check`
