# GC Production Troubleshooting

## Objective

Production testing should turn every confirmed failure into a deterministic regression at the narrowest owning layer. Preserve the original build commit, runtime role, message sequence, lobby identity, and structured diagnostics before changing code.

## Minimum Incident Record

Capture:

- Build commit and target architecture.
- Client or gameserver role.
- Dota and emulator configuration relevant to the flow.
- Reproduction steps with timing-sensitive actions noted.
- Inner message ID and direct or wrapped path.
- Lobby ID and generation.
- Server ID and endpoint when reconnect is involved.
- Source and target job IDs when present.
- Structured GC log around the failure.
- Expected and observed protocol-visible behavior.

## GC Log

The Windows GC debug log path is:

```text
C:\Users\Public\gbe_gc_debug.log
```

The logger appends records. A missing log usually points to DLL deployment, process loading, filesystem permissions, architecture mismatch, or security software interference.

Typed reconnect and lifecycle events use stable fields:

```text
event=<event>
reason=<reason>
source=<source>
lobby_id=<value>
generation=<value>
server_id=<value>
endpoint=<value-or-dash>
decision=<value-or-dash>
message_id=<value-or-dash>
job_id=<value-or-dash>
```

Primary scopes:

- `GBE_RECONNECT_EVENT`.
- `GBE_LIFECYCLE_EVENT`.
- `GC_CALLBACK`.
- `GC_DOTA_LOBBY`.
- `GC_DOTA_PATCH`.
- `GC_DOTA_HELLO` and `GC_DOTA_SERVER_HELLO`.
- `GC_DOTA_WELCOME` and `GC_DOTA_SYNC`.

## Fault Isolation Path

Trace a request through this sequence:

```text
message ID
  -> direct or wrapped parser
  -> typed registry or explicit special route
  -> domain handler
  -> lifecycle transition or pure planner
  -> ordered effect list
  -> executor
  -> Store, pending queue, callback queue, or network adapter
  -> incoming message retrieval
```

At each step, identify the canonical owner before changing code.

## Stale Generation

Symptoms:

- Delayed state update disappears after leave/rejoin.
- Reconnect callback is queued but never delivered.
- Deferred teardown does not clear the current lobby.
- Logs report `stale_generation`.

Compare:

```text
queued_lobby_id
queued_generation
current_lobby_id
current_generation
```

A stale rejection is expected when the lifecycle changed. A defect exists when current-generation work is rejected, stale work mutates current state, or generation was captured after queueing.

Regression target:

- Handler test for protocol ordering.
- Reconnect test for callback or dedup behavior.
- Store test for stale publication.
- Lifecycle test for transition generation.
- Concurrency stress for cross-thread history.

## Reconnect Failure

Check in this order:

1. Context source selection: shared, recent, local, or generic recovery.
2. Reconnect eligibility.
3. Active/started state.
4. Custom game and local participant/owner rules.
5. Server ID and endpoint presence.
6. IPv4 endpoint parsing.
7. Current generation.
8. Direct-connect and callback dedup keys.
9. Lock release before external effects.
10. Generation guard at callback dispatch.

Common typed reasons include:

- `no_context`.
- `ordinary_practice_lobby`.
- `reconnect_ineligible`.
- `state_not_ready`.
- `local_owner`.
- `missing_server_id`.
- `missing_endpoint`.
- `already_queued`.
- `stale_generation`.

Use `gbe_dota_reconnect_network_test` for source, eligibility, dedup, connector, callback, and lock-boundary regressions.

## Lobby State Drift

Symptoms:

- Client and gameserver disagree about lobby members, server, launch phase, or custom game.
- A cleared lobby reappears.
- A stale writer overwrites a replacement lobby.

Check:

- Both roles reference the same application Store.
- Each business operation uses one immutable snapshot.
- Shared publication uses current-or-newer generation semantics.
- Partial update uses exact-generation compare/update.
- Abandon suppression remains active where required.
- Client adoption validates owner/member participation.
- Recent reconnect context is recorded only after successful Store publication.

Use Store and composition-root tests to reproduce this class of failure.

## Protocol Order Regression

Symptoms:

- Correct messages arrive in the wrong order.
- Teardown completes before the client retrieves an expected message.
- Wrapped response loses its session.
- A conditional follow-up executes after a failed prerequisite.

Check:

- Registry mode and session policy.
- `GBE_DotaActionList` order.
- `only_when_previous_action_succeeded` and runtime-update conditions.
- Push route and wrapped response adaptation.
- Retrieval-driven teardown finalization.
- Replay summary changes.

Use handler tests for exact action order and replay fixtures for protocol summaries.

## Parse Or Template Patch Failure

Relevant log phrases include:

```text
semantic rewrite skipped full parse
no semantic account_id varint replacements
donor does not expose expected varint
size mismatch skipped
fixed64 patch skipped
connect size changed
relying on proto rewrite
```

Capture encoded size, donor size, target field, request/response IDs, stage, and payload prefix. Avoid logging complete sensitive payloads.

Use wire tests, payload helper tests, and `wire_edge_cases` replay for regression coverage.

## Concurrency Or Lifetime Failure

Signals:

- TSAN report.
- Callback observes destroyed state.
- Separate client/gameserver instances share reconnect mutation.
- Deadlock around reconnect or callback execution.

Required lock order:

```text
global_mutex
  -> serialized instance mutex
  -> prepare and reserve dedup
  -> release instance mutex
  -> release global_mutex
  -> execute external effects
```

Run:

```bash
CXX=clang++ bash tools/run_gc_tsan_tests.sh
```

Also use composition-root destruction/recreation tests and the bounded concurrency stress test.

## Production Build Failure

The PR production gates are Windows and Linux `api_experimental` x64 release builds. Focused tests can pass while production integration fails because production targets compile a broader source graph and use platform-specific toolchains.

Local Linux project generation may fail when the bundled Premake requires GLIBC 2.38 and the host provides an older glibc. Preserve the CI logs and treat the blocking Linux job as the production authority for that environment.

## Converting An Incident Into A Regression

Choose the narrowest durable layer:

| Incident | Regression form |
| --- | --- |
| Pure transition mismatch | Lifecycle example/property test |
| Sequence-dependent lifecycle defect | Differential model or deterministic sequence |
| Stale async work | Leave/rejoin generation regression |
| Reconnect connector/callback defect | Reconnect fake integration test |
| Store overwrite or torn snapshot | Store or concurrency stress test |
| Side-effect order | Handler executor test |
| Wire or payload drift | Payload helper or wire test |
| Multi-message protocol drift | Replay fixture |
| Architecture bypass | Positive and negative audit fixture |

Keep the production fix local to the owner exposed by the regression.
