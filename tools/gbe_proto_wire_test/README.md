# gbe_proto_wire_test

This test target covers byte-level helpers that are independent from generated
protobuf headers.

## Current Layout

- `gbe_proto_wire_test.cpp` contains the test runner, shared expectations,
  legacy baselines, and three grouped test functions.
- `test_basic_wire_and_parsers()` covers generic protobuf wire helpers,
  parsing helpers, string helpers, and small Dota wire rules.
- `test_patch_and_rewrite_helpers()` covers template patching, recursive varint
  rewriting, and Dota lobby object rewrite helpers.
- `test_summary_helpers()` covers summary/formatting helpers used by GC logs.

## Split Plan

Keep one executable target and split tests by behavior when the next large test
edit is needed:

- `test_support.h/.cpp`: shared expectations, hex helpers, and legacy baselines.
- `proto_wire_basic_test.cpp`: generic wire, parsing, and string helpers.
- `proto_wire_patch_test.cpp`: patch/rewrite helpers and Dota GC rewrite helpers.
- `proto_wire_summary_test.cpp`: summary and formatting helpers.
- `gbe_proto_wire_test.cpp`: `main()` only.

Each extracted file should expose one `bool test_*()` function and keep byte-level
fixtures local to that file.
