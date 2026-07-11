# GC Testing And Verification

## Verification Layers

The GC verification strategy combines focused logic tests, production-path fakes, replay fixtures, properties, differential models, structural audits, production builds, and ThreadSanitizer.

| Layer | Purpose |
| --- | --- |
| Header compile checks | Detect hidden include-order and dependency coupling |
| Focused offline tests | Validate domain logic, adapters, Store, routing, and side-effect order |
| Replay fixtures | Protect protocol summaries and representative message flows |
| Property tests | Exercise generated deterministic histories and invariants |
| Differential model | Compare lifecycle implementation with an independent reference model |
| Architecture audits | Block ownership, mapping, source-list, and CI regressions |
| Production builds | Validate real Windows/Linux target integration |
| Clang TSAN | Detect races in reconnect and shared-state concurrency boundaries |

## Commands

Fast iteration:

```bash
bash tools/run_gc_verification.sh --fast
```

Full delivery gate:

```bash
CXX=c++ bash tools/run_gc_verification.sh --full --base-sha origin/dev
```

Clang full verification:

```bash
CXX=clang++ bash tools/run_gc_verification.sh --full --base-sha origin/dev
```

ThreadSanitizer:

```bash
CXX=clang++ bash tools/run_gc_tsan_tests.sh
```

Audit regression suite:

```bash
python3 tools/test_audit_gc_refactor.py
```

Production architecture audit:

```bash
python3 tools/_audit_gc_refactor.py
```

## Fast And Full Coverage

Fast verification includes:

- Eight standalone header compile checks.
- Audit helper regression tests.
- Message utilities and GC config.
- Reconnect network/state and callback execution guard.
- Shared lobby Store.
- Composition-root ownership tests.
- Lifecycle state-machine examples, properties, and differential model.
- Bounded concurrency stress.
- Typed handler registry.
- Wire and router helpers.
- Four high-signal replay fixtures.
- Payload helper tests.
- Handler smoke and executor tests.
- Production architecture audit.
- `git diff --check`.

Full verification adds:

- Three additional replay fixtures.
- Lobby flow focused tests.
- Lobby state focused tests.
- Custom-game focused tests.

## Test Selection

| Change area | Primary tests |
| --- | --- |
| Wrapper, session, source-job metadata | `gc_message_utils_test` |
| Wire encoding and parsing | `gbe_proto_wire_test` |
| GC configuration | `gbe_gc_config_test` |
| Reconnect source, generation, dedup, effects | `gbe_dota_reconnect_network_test` |
| Callback lock boundary | `callsystem_execution_guard_test` |
| Shared lobby Store | `gbe_dota_lobby_state_store_test` |
| Ownership and role isolation | `gbe_dota_composition_root_test` |
| Lifecycle transitions | `gbe_dota_lifecycle_state_machine_test` |
| Cross-thread Store and reconnect history | `gbe_dota_concurrency_stress_test` and TSAN |
| Typed post-login mapping | `gbe_dota_handler_registry_test` |
| Payload transformations | `gbe_dota_gc_payload_helpers_test` |
| Handler orchestration and effect ordering | `gbe_dota_handler_test` |
| Chat behavior | `chat_channel` replay |
| Lobby teardown and signout | `lobby_lifecycle` replay |
| Custom-game metadata | `gbe_dota_custom_game_test` and `practice_lobby` replay |
| Wire edge cases | `wire_edge_cases` replay |

## Replay Fixtures

Fast fixtures:

- `minimal`.
- `practice_lobby`.
- `game_flow`.
- `cache_and_items`.

Full-only fixtures:

- `chat_channel`.
- `lobby_lifecycle`.
- `wire_edge_cases`.

Fixture sources and expected summaries are under `tools/gc_replay_test/fixtures/`.

## Lifecycle Model Coverage

The lifecycle suite protects:

- Eight lifecycle states.
- Thirteen event kinds.
- All 104 state/event pairs.
- Compile-time transition-table completeness.
- 100,000 deterministic length-five sequences.
- Every stale-generation state/event combination.
- Eight fixed differential seeds with 4,096 total steps.

When changing lifecycle vocabulary or semantics, update examples, properties, the independent differential model, production-path tests, and architecture evidence together.

## ThreadSanitizer

`tools/run_gc_tsan_tests.sh` builds:

- `gbe_dota_reconnect_network_test`.
- `gbe_dota_concurrency_stress_test`.

It uses Clang, `-fsanitize=thread`, frame pointers, debug information, and `TSAN_OPTIONS=halt_on_error=1:exitcode=66`. The first race report fails the gate.

## Architecture Audits

Audit 1-27 protect these categories:

| Audits | Boundary |
| --- | --- |
| 1-3 | Declaration/definition and documentation consistency |
| 4-6 | Typed registry, template ownership, and source-list inclusion |
| 7-9 | Handler side effects, diagnostic reasons, and lifecycle effect ownership |
| 10-14 | Shared Store, concurrency contract, retired layers, and canonical owners |
| 15-18 | Composition-root lifecycle, mutable globals, layered CI, and transition gates |
| 19 | Versioned architecture investment inputs |
| 20-24 | Handler responsibility, state/effect ownership, object lifecycle, state machine, and generation-scoped async work |
| 25-26 | High-risk test credibility and CI failure localization |
| 27 | Actor, formal-model, and model-CI decision consistency |

Audit helper fixtures must include positive and negative cases for new structural rules.

## CI Gates

The PR workflow exposes four independent blocking jobs:

| Job ID | Display name | Coverage |
| --- | --- | --- |
| `emu-win-release` | `win` | Windows `api_experimental` x64 release |
| `emu-linux-release` | `linux` | Linux `api_experimental` x64 release |
| `gc-verification` | `gc verification` | Fast offline, audit, and PR-base diff check |
| `gc-tsan` | `gc thread sanitizer` | Clang TSAN focused suite |

Pure Markdown changes are ignored by this workflow. Documentation-only changes require local review and `git diff --check`.

## Final Refactor Baseline

The completed P16 acceptance record reports:

- Audit helper: 73 of 73.
- Production audits: 27 groups with zero issues.
- Reconnect assertions: 772 of 772.
- Callsystem guard assertions: 8 of 8.
- Registry assertions: 339 of 339.
- Payload assertions: 546 of 546.
- Handler tests: 78 of 78.
- Replay fixtures: 7 of 7.
- GCC full verification: passed.
- Clang ThreadSanitizer: passed with zero race reports.

Treat these counts as a recorded acceptance baseline. Update them when tests are intentionally added or removed; use pass/fail behavior and protected invariants as the primary maintenance signal.
