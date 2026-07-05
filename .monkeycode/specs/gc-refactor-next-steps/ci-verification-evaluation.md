# GC Refactor CI Verification Evaluation

This note evaluates which GC refactor checks are suitable for CI.

## CI-Safe Checks

These checks use local source files, bundled fixtures, and the system C++ compiler. They do not require Steam credentials, network services, generated protobuf downloads, or external game state.

```bash
tools/run_gc_offline_tests.sh --full
python3 tools/_audit_gc_refactor.py
git diff --check
```

Recommended full wrapper for pre-merge or scheduled CI:

```bash
tools/run_gc_verification.sh
```

## Suggested CI Trigger

Run the GC verification job on pull requests when these paths change:

- `dll/gbe_dota_*.cpp`
- `dll/gbe_dota_*.h`
- `dll/gbe_proto_wire.cpp`
- `dll/gbe_proto_wire.h`
- `dll/gbe_gc_message_utils.cpp`
- `dll/gbe_gc_message_utils.h`
- `dll/dll/steam_game_coordinator.h`
- `dll/dll/gbe_dota_reconnect_shared.h`
- `tools/gc_*`
- `tools/gbe_*_test/**`
- `tools/run_gc_offline_tests.sh`
- `tools/run_gc_verification.sh`
- `tools/_audit_gc_refactor.py`

## Premake Gate

The project already exposes a GC test generation option:

```bash
./premake5 --with-gc-tests gmake2
```

Use this gate in CI only when the runner image has a compatible `premake5` binary and the generated build target can compile with available dependencies. This is most valuable for changes to:

- `premake5.lua`
- build file membership
- test project membership
- include path or source list layout

## Deferred CI Items

These items should stay outside the first CI pass:

- Networked Steam/Game Coordinator integration scenarios
- Tests requiring real credentials, tokens, or private services
- Platform matrix expansion beyond the existing compiler available to the runner
- Premake target builds until the runner image and dependency cache are confirmed

## Recommendation

Add a dedicated lightweight PR verification workflow or job that runs `tools/run_gc_verification.sh --fast` for the path set above. Run `tools/run_gc_verification.sh` in pre-merge or scheduled CI. Add the Premake gate as a separate optional job after confirming `premake5` availability in the CI image.
