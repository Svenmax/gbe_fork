#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/opencode/gbe_gc_tsan_tests"
CXX_BIN="${CXX:-clang++}"
COMMON_FLAGS=(-std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=thread -pthread -I. -Isdk -Ilibs)

mkdir -p "$BUILD_DIR"
cd "$ROOT_DIR"

build_and_run() {
    local name="$1"
    shift

    printf '[tsan-build] %s\n' "$name"
    "$CXX_BIN" "${COMMON_FLAGS[@]}" "$@" -o "$BUILD_DIR/$name"

    printf '[tsan-run] %s\n' "$name"
    TSAN_OPTIONS="halt_on_error=1:exitcode=66" "$BUILD_DIR/$name"
}

build_and_run \
    gbe_dota_reconnect_network_test \
    tools/gbe_dota_reconnect_network_test/gbe_dota_reconnect_network_test.cpp \
    dll/gbe_dota_reconnect_network.cpp \
    dll/gbe_dota_serialized_connection_state.cpp \
    dll/gbe_proto_wire.cpp

build_and_run \
    gbe_dota_concurrency_stress_test \
    tools/gbe_dota_concurrency_stress_test/gbe_dota_concurrency_stress_test.cpp \
    dll/gbe_dota_lobby_state_store.cpp \
    dll/gbe_dota_reconnect_context.cpp \
    dll/gbe_dota_chat_flow.cpp \
    dll/gbe_dota_lobby_launch_flow.cpp \
    dll/gbe_dota_lobby_member_flow.cpp \
    dll/gbe_dota_lobby_payload_flow.cpp \
    dll/gbe_dota_lobby_flow.cpp \
    dll/gbe_dota_lobby_state.cpp \
    dll/gbe_dota_custom_game.cpp \
    dll/gbe_dota_gc_wire.cpp \
    dll/gbe_proto_wire.cpp

printf 'GC ThreadSanitizer tests passed\n'
