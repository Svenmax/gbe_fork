# gc-audit-function-call-helper requirements

## Scope

Extract repeated audit-only function-call presence regex checks into one shared Python helper.

## Requirements

- The helper shall detect a symbol followed by optional whitespace and `(` at a word boundary.
- The helper shall escape symbol text before building the regex.
- Lifecycle and reconnect ownership audits shall use the helper for same-shape call checks.
- Existing issue strings and production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change audit ownership boundaries.
- Do not change production C++ routing or lifecycle behavior.
- Do not broaden symbol matching beyond the existing call-token regex semantics.
