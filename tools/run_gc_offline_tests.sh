#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/opencode/gbe_gc_offline_tests"
CXX_BIN="${CXX:-c++}"

mkdir -p "$BUILD_DIR"
cd "$ROOT_DIR"

COMMON_FLAGS=(-std=c++17 -I. -Ilibs)

build_and_run() {
    local name="$1"
    shift

    printf '[build] %s\n' "$name"
    "$CXX_BIN" "${COMMON_FLAGS[@]}" "$@" -o "$BUILD_DIR/$name"

    printf '[run] %s\n' "$name"
    "$BUILD_DIR/$name"
}

build_and_run \
    gc_message_utils_test \
    tools/gc_message_utils_test/gc_message_utils_test.cpp \
    dll/gbe_gc_message_utils.cpp \
    dll/gbe_proto_wire.cpp

build_and_run \
    gbe_gc_config_test \
    tools/gbe_gc_config_test/gbe_gc_config_test.cpp \
    dll/gbe_gc_config.cpp

build_and_run \
    gbe_proto_wire_test \
    tools/gbe_proto_wire_test/gbe_proto_wire_test.cpp \
    dll/gbe_proto_wire.cpp \
    dll/gbe_dota_gc_wire.cpp \
    dll/gbe_dota_gc_router.cpp \
    dll/gbe_dota_lobby_state.cpp \
    dll/gbe_dota_lobby_flow.cpp \
    dll/gbe_dota_custom_game.cpp \
    dll/gbe_dota_custom_lobby_http.cpp \
    dll/gbe_gc_message_utils.cpp

printf '[build] %s\n' gc_replay_test
"$CXX_BIN" "${COMMON_FLAGS[@]}" \
    tools/gc_replay_test/gc_replay_test.cpp \
    tools/gc_replay_test/gc_replay_summary.cpp \
    dll/gbe_proto_wire.cpp \
    dll/gbe_gc_message_utils.cpp \
    -o "$BUILD_DIR/gc_replay_test"

printf '[run] %s minimal\n' gc_replay_test
"$BUILD_DIR/gc_replay_test" \
    tools/gc_replay_test/fixtures/minimal.txt \
    --expect tools/gc_replay_test/fixtures/minimal.expected.txt

printf '[run] %s practice_lobby\n' gc_replay_test
"$BUILD_DIR/gc_replay_test" \
    tools/gc_replay_test/fixtures/practice_lobby.txt \
    --expect tools/gc_replay_test/fixtures/practice_lobby.expected.txt

build_and_run \
    gbe_dota_lobby_flow_test \
    tools/gbe_dota_lobby_flow_test/gbe_dota_lobby_flow_test.cpp \
    dll/gbe_dota_lobby_flow.cpp \
    dll/gbe_dota_lobby_publish.cpp \
    dll/gbe_dota_lobby_snapshot.cpp \
    dll/gbe_dota_custom_game.cpp \
    dll/gbe_proto_wire.cpp

build_and_run \
    gbe_dota_custom_game_test \
    tools/gbe_dota_custom_game_test/gbe_dota_custom_game_test.cpp \
    dll/gbe_dota_custom_game.cpp \
    dll/gbe_dota_custom_lobby_http.cpp \
    dll/gbe_proto_wire.cpp

printf '[build] %s\n' gbe_dota_gc_payload_helpers_test
"$CXX_BIN" "${COMMON_FLAGS[@]}" \
    -Itools/gbe_dota_gc_payload_helpers_test \
    -Itools/gbe_dota_gc_payload_helpers_test/pb_stubs \
    tools/gbe_dota_gc_payload_helpers_test/test_wrapper.cpp \
    tools/gbe_dota_gc_payload_helpers_test/gbe_dota_gc_payload_helpers_test.cpp \
    dll/gbe_proto_wire.cpp \
    dll/gbe_gc_message_utils.cpp \
    dll/gbe_dota_lobby_flow.cpp \
    dll/gbe_dota_lobby_publish.cpp \
    dll/gbe_dota_lobby_snapshot.cpp \
    dll/gbe_dota_custom_game.cpp \
    dll/gbe_dota_custom_lobby_http.cpp \
    dll/gbe_dota_gc_wire.cpp \
    dll/gbe_dota_gc_router.cpp \
    -o "$BUILD_DIR/gbe_dota_gc_payload_helpers_test"

printf '[run] %s\n' gbe_dota_gc_payload_helpers_test
"$BUILD_DIR/gbe_dota_gc_payload_helpers_test"

printf 'all GC offline tests passed\n'
