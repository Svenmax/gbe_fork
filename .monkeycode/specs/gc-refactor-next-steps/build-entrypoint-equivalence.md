# GC Build Entrypoint Equivalence

## Source Entrypoints

| Entrypoint | Role | GC Source Inclusion Rule |
| --- | --- | --- |
| `premake5.lua` production projects | Production builds for `api_regular`, `api_experimental`, `steamclient_experimental`, `steamclient_regular`, and related targets | `common_files` includes `dll/**`, so new production `dll/*.cpp` and headers are automatically included unless a target-specific `removefiles` rule excludes them |
| `tools/run_gc_offline_tests.sh` | Local Linux offline GC compile/run harness | Each test binary lists the exact `.cpp` files it links; new production helper `.cpp` files must be added to every affected test binary in this script |
| `premake5.lua` always-on GC utility tests | Optional tool targets present in the generated project set by default | `tool_gc_replay_test`, `tool_gc_message_utils_test`, `tool_gbe_gc_config_test`, `tool_gbe_proto_wire_test`, `tool_gbe_dota_custom_game_test`, `tool_gbe_dota_lobby_flow_test`, and `tool_gbe_dota_lobby_state_test` are declared outside `--with-gc-tests` |
| `premake5.lua --with-gc-tests` | Optional heavy handler/payload test project generation | `tool_gbe_dota_gc_payload_helpers_test` and `tool_gbe_dota_handler_test` are declared only inside `if _OPTIONS["with-gc-tests"] then`; test-only `.cpp` files belong here and in `tools/run_gc_offline_tests.sh` only |
| `.github/workflows/emu-build-all-linux.yml` | Production Linux build matrix | Generates project files with `premake5.lua --genproto --os=linux gmake`; it does not pass `--with-gc-tests` and builds production/tool matrix targets only |
| `.github/workflows/emu-build-all-win.yml` | Production Windows build matrix | Generates project files with `premake5.lua --genproto --os=windows vs2026`; it does not pass `--with-gc-tests` and builds production matrix targets only |

## New File Placement Rules

- New production GC `.cpp` under `dll/` enters production Premake builds automatically through `common_files`.
- New production GC `.cpp` must be manually added to `tools/run_gc_offline_tests.sh` for each offline binary that needs the symbol.
- New production GC `.cpp` used by optional Premake test targets must be manually added to the matching `files {}` list in `premake5.lua`.
- New handler or payload test-only `.cpp` files must stay out of production targets; add them only to `tools/run_gc_offline_tests.sh` and the `--with-gc-tests` Premake block.
- New headers used by optional Premake test targets should be listed in `premake5.lua` for IDE/project visibility even when compilation would work through includes.

## Premake Default Path

Default `premake5.lua` generation keeps the two heavy handler/payload GC test projects out of the project set because they are guarded by `--with-gc-tests`. This preserves the production workflow matrix cost. The always-on GC utility tests remain regular tool projects and can be built explicitly from generated projects.

## Platform Differences

| Risk | Linux Path | Windows Path | Current Mitigation |
| --- | --- | --- | --- |
| Project generator action | `gmake` | `vs2026` | Keep source lists in `premake5.lua` rather than generated project files |
| Production source inclusion | `common_files` + target `removefiles` | `common_files` + target `removefiles` | Production GC files should live under `dll/` and avoid target-specific OS assumptions |
| Offline GC verification | `tools/run_gc_offline_tests.sh` uses local C++ compiler | No equivalent workflow in current repo | Treat shell offline tests as Linux gate; Premake `--with-gc-tests` is the cross-project generation gate when tools are available |
| Include order | Shell tests pass explicit `-I. -Isdk -Ilibs` plus test stub includes | Premake targets inherit project include dirs and generated proto dirs | Add source dependencies explicitly to shell and Premake test targets when introducing new helper `.cpp` files |
| Link order | Shell tests link direct source files in command order | Premake resolves project source compilation through generated build files | Keep helper code free of hidden link-only dependencies; verify with `tools/run_gc_verification.sh` after source-list changes |

## CI Smoke Gate Evaluation

Recommended first PR gate:

```bash
tools/run_gc_verification.sh --fast
```

This gate is credential-free and avoids Premake/dependency setup. It covers fast offline GC binaries, audit checks, and `git diff --check`. Full offline verification is better for pre-merge or scheduled jobs:

```bash
tools/run_gc_verification.sh
```

Premake GC test generation should remain a separate optional job until CI images reliably expose the checked-in third-party `premake5` binaries and the expected dependency directories.
