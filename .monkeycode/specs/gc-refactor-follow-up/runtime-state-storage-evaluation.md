# GC Follow-Up Runtime State Storage Evaluation

## Scope

This note evaluates whether the small runtime state groups stabilized by Stage 4 should move into a dedicated storage struct.

## Stable Accessor Groups

The following state groups now have accessor APIs:

- Host showcase equip repush flag.
- Server hello cache.
- Login sync sent flag.
- Private lobby snapshot replayed flag.
- Last launch state pushed game state.
- Launch persona dedup signature.
- Direct-connect callback dedup signature.

## Recommendation

Defer creating a new storage struct until one more functional refactor stage lands.

Rationale:

- Accessor APIs already isolate direct reads and writes, so callers can remain stable if storage moves later.
- The current fields are still spread across launch, welcome, inventory, and lobby snapshot behavior; forcing a struct now would mostly move data without clarifying ownership.
- `GBE_local_lobby` and `GBE_shared_dota_lobby_state` remain intentionally outside this evaluation because they are broad runtime models with many behavioral dependencies.
- A later struct should contain only small stabilized flags and signatures, with existing accessor names preserved.

## Future Struct Shape

If this is revisited, prefer a narrow member such as:

```cpp
struct GBE_DotaRuntimeFlags
{
    bool login_sync_sent{};
    bool host_showcase_equip_pushed{};
    bool private_lobby_snapshot_replayed{};
    uint32 last_launch_state_pushed_game_state{};
    std::string last_launch_persona_signature;
    std::string last_direct_connect_callback_signature;
};
```

The server hello cache can either stay separate or move into a similarly narrow cache struct because it carries a full context payload rather than a simple flag/signature.

## Migration Criteria

Create the storage struct only when all of these are true:

- No direct access remains outside accessor implementations.
- At least one additional refactor stage benefits from grouped reset or grouped diagnostics.
- The migration preserves all existing accessor names and semantics.
- `tools/run_gc_verification.sh` passes after the storage move.
