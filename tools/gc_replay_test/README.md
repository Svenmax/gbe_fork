# gc_replay_test

`gc_replay_test` is a small offline verification tool for GC message fixtures.

It reads text fixtures, decodes hex payloads, and prints stable summaries that can be used as golden files during refactors.

## Fixture format

Each non-empty line has this format:

```text
<emsg> <hex_payload> [label]
```

Examples:

```text
2147483652 08011203616263 client_welcome_body
26 08011002 lobby_update_body
```

Hex payloads may use `0x` or `hex=` prefixes and may include `_`, `-`, or `:` separators.

## Usage

Generate a summary:

```bash
gc_replay_test_x64 fixture.txt > expected.txt
```

Compare against a golden summary:

```bash
gc_replay_test_x64 fixture.txt --expect expected.txt
```

Generate machine-readable JSONL:

```bash
gc_replay_test_x64 fixture.txt --format jsonl
```

The first version summarizes message metadata, FNV-1a payload hash, and top-level protobuf field layout as `field:wire_type:value_size=value`.

## Smoke test

After building the tool, run the bundled fixture:

```bash
gc_replay_test_x64 tools/gc_replay_test/fixtures/minimal.txt --expect tools/gc_replay_test/fixtures/minimal.expected.txt
```

Manual build commands for the current offline guardrails:

```bash
c++ -std=c++17 -I. tools/gc_replay_test/gc_replay_test.cpp tools/gc_replay_test/gc_replay_summary.cpp dll/gbe_proto_wire.cpp dll/gbe_gc_message_utils.cpp -o /tmp/opencode/gc_replay_test
/tmp/opencode/gc_replay_test tools/gc_replay_test/fixtures/minimal.txt --expect tools/gc_replay_test/fixtures/minimal.expected.txt

c++ -std=c++17 -I. tools/gc_message_utils_test/gc_message_utils_test.cpp dll/gbe_gc_message_utils.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gc_message_utils_test
/tmp/opencode/gc_message_utils_test

c++ -std=c++17 -I. tools/gbe_proto_wire_test/gbe_proto_wire_test.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gbe_proto_wire_test
/tmp/opencode/gbe_proto_wire_test
```
