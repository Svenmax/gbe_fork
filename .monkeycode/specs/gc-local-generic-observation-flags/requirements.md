# gc-local-generic-observation-flags requirements

## Scope

Reduce Local observation flag write scatter in generic lobby kick and owner adoption handling.

## Requirements

- Generic lobby kick/adoption observation flags shall be written through `gbe::dota_lobby_state` helpers.
- Log-once helpers shall return whether the flag changed so existing diagnostics remain single-shot.
- Existing kick detection, owner adoption decisions, push/reset behavior, and diagnostics order shall remain unchanged.

## Non-Goals

- Do not change generic lobby membership lookup.
- Do not change kick detection policy.
- Do not change owner adoption policy.
