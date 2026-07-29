# gc-local-cache-subscription-metadata-apply design

## Current State

`GBE_RecordDotaLobbyCacheSubscriptionState(...)` validates the CacheSubscribed owner SOID, then directly writes cache metadata fields on `GBE_local_lobby` before logging and publishing shared lobby state.

## Design

- Add `apply_cache_subscription_metadata(...)` to `gbe::dota_lobby_state`.
- Keep protobuf parsing and owner SOID guards in `gbe_dota_lobby_state_publish_coordinator.cpp`.
- Convert `service_list` to a local vector before calling the helper.
- Preserve the existing logging and publish sequence after Local has been updated.
- Add focused tests for initial metadata apply, no-op apply, and optional-field clearing behavior.

## Validation

- `git diff --check`
- `bash tools/run_gc_verification.sh --full`
