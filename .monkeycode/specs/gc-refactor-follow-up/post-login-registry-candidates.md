# GC Follow-Up Post-Login Registry Candidates

## Scope

This note selects the next small direct-only handlers for Stage 5 registry migration.

## Selected Batch

Migrate these direct-path request handlers first:

- `7534` -> `GBE_HandleDotaProfileCardRequest`
- `2581` -> `GBE_HandleDotaLookupAccountNameRequest`
- `7503` -> `GBE_HandleDotaEmoticonDataRequest`
- `8095` -> `GBE_HandleDotaConductScorecardRequest`
- `8800` -> `GBE_HandleDotaCoachingSummaryRequest`

Selection rationale:

- They are direct-only branches in `GBE_HandleDotaDirectPostLoginRequest`.
- They already use the common `body/body_size/has_source_job/source_job` signature shape.
- They do not carry wrapped-session semantics.
- They do not mutate launch, lobby, reconnect, inventory persistence, network broadcast, or template replay state.
- They are narrow enough to migrate in one registry batch while keeping review risk low.

## Deferred Paths

Keep these as explicit branches for now:

- Template replay fallback and all large canned replay/template paths.
- Launch chain packets such as `4506`, `5429`, `8870`, `4511`, and `4508`.
- Lobby lifecycle and abandon/signout flows.
- Inventory mutation handlers such as equip/unlock/set style.
- Cache subscription refresh and `7034` runtime state paths.
- Chat and lobby broadcast channel paths that keep custom logging or wrapped variants.

## Migration Requirements

For Stage 5.2:

- Add captureless adapters in `GBE_DispatchDotaPostLoginRequest`.
- Preserve direct path only by checking `context.path == Direct`.
- Pass `context.body.data()`, `context.body.size()`, `context.has_request_job`, and `context.request_job_id` unchanged.
- Remove only the matching explicit `if` branches after the table entries pass verification.
- Run `tools/run_gc_verification.sh` after the migration.

For Stage 5.3:

- Update `tools/_audit_gc_refactor.py` expected post-login mapping for the new table entries.
- Add or extend audit coverage so each migrated entry keeps the direct-only guard.
