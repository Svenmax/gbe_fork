# gc-audit-adapter-handler-call-helper requirements

## Scope

Extract audit-only registry adapter request-handler call parsing into one named Python helper.

## Requirements

- The helper shall return every `self->GBE_HandleDota...Request(` handler symbol in adapter body order.
- The helper shall return an empty list when no adapter handler call exists.
- Post-login dispatch audit shall use the helper while preserving existing issue strings.
- Production C++ behavior shall remain unchanged.

## Non-Goals

- Do not change routing truth tables.
- Do not change registry entries or adapter implementations.
- Do not broaden the parser beyond the existing `self->GBE_HandleDota...Request(` shape.
