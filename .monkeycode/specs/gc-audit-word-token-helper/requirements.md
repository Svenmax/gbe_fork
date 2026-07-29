# gc-audit-word-token-helper requirements

## Scope

Extract repeated audit-only whole-word token presence checks into one shared Python helper.

## Requirements

- The helper shall detect an escaped token between regex word boundaries.
- The helper shall return false when the token only appears as part of a longer word.
- Retired lifecycle, reconnect, and shared-lobby compatibility audits shall use the helper for same-shape token checks.
- Existing issue strings and production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change retired token inventories.
- Do not change production C++ behavior.
- Do not broaden matching beyond the existing `\b token \b` regex semantics.
