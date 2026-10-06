# gc-local-cache-subscription-metadata-apply requirements

## Scope

Reduce Local lobby write scatter by moving parsed CacheSubscribed metadata field writes behind a named state helper.

## Requirements

- `GBE_RecordDotaLobbyCacheSubscriptionState(...)` shall apply cache metadata through a `gbe::dota_lobby_state` helper after parsing and owner SOID validation.
- The helper shall update version, service id, service list, and sync version as one field group.
- Missing optional fields shall continue to clear their corresponding present flags and values.
- Existing debug log, summary log, and shared publish ordering shall remain unchanged.

## Non-Goals

- Do not change CacheSubscribed protobuf parsing.
- Do not change owner SOID validation.
- Do not move shared Store ownership.
