# gc-inventory-multi-numeric-cell-helper requirements

## Scope

Extract repeated multi-number Markdown cell parsing used by GC routing inventory audits into one shared Python helper.

## Requirements

- The audit helper shall return every decimal emsg token found in a Markdown table cell.
- The audit helper shall preserve the existing token order from the cell.
- The template-only inventory audit shall use the shared helper when parsing multi-emsg cells.
- Existing issue strings and production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change routing truth tables.
- Do not change production C++ routing behavior.
- Do not broaden template-only inventory matching beyond the existing `TEMPLATE_ONLY` section logic.
