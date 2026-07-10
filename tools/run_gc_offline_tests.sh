#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/opencode/gbe_gc_offline_tests"
CXX_BIN="${CXX:-c++}"
FULL=0

usage() {
    printf 'usage: %s [--full]\n' "$(basename "$0")"
    printf '  default  run fast high-signal GC offline tests\n'
    printf '  --full   run the complete GC offline test suite\n'
}

for arg in "$@"; do
    case "$arg" in
        --full)
            FULL=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
done

mkdir -p "$BUILD_DIR"
cd "$ROOT_DIR"

COMMON_FLAGS=(-std=c++17 -I. -Isdk -Ilibs)

build_and_run() {
    local name="$1"
    shift

    printf '[build] %s\n' "$name"
    "$CXX_BIN" "${COMMON_FLAGS[@]}" "$@" -o "$BUILD_DIR/$name"

    printf '[run] %s\n' "$name"
    "$BUILD_DIR/$name"
}

compile_header_test() {
    local name="$1"
    local source="$2"

    printf '[build] %s\n' "$name"
    "$CXX_BIN" "${COMMON_FLAGS[@]}" -c "$source" -o "$BUILD_DIR/$name.o"
}

compile_header_test \
    reconnect_shared_header_compile \
    tools/gbe_dota_header_compile_test/reconnect_shared_header_compile.cpp

compile_header_test \
    serialized_connection_state_header_compile \
    tools/gbe_dota_header_compile_test/serialized_connection_state_header_compile.cpp

compile_header_test \
    reconnect_network_header_compile \
    tools/gbe_dota_header_compile_test/reconnect_network_header_compile.cpp

compile_header_test \
    lobby_state_store_header_compile \
    tools/gbe_dota_header_compile_test/lobby_state_store_header_compile.cpp

compile_header_test \
    handler_registry_header_compile \
    tools/gbe_dota_header_compile_test/handler_registry_header_compile.cpp

compile_header_test \
    diagnostic_event_header_compile \
    tools/gbe_dota_header_compile_test/diagnostic_event_header_compile.cpp

printf '[run] %s\n' audit_gc_refactor_test
python3 tools/test_audit_gc_refactor.py

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
    gbe_dota_reconnect_network_test \
    -pthread \
    tools/gbe_dota_reconnect_network_test/gbe_dota_reconnect_network_test.cpp \
    dll/gbe_dota_reconnect_network.cpp \
    dll/gbe_dota_serialized_connection_state.cpp \
    dll/gbe_proto_wire.cpp

build_and_run \
    callsystem_execution_guard_test \
    -Idll \
    -DGBE_CALLSYSTEM_STANDALONE_TEST \
    tools/callsystem_execution_guard_test/callsystem_execution_guard_test.cpp \
    dll/callsystem.cpp

build_and_run \
    gbe_dota_lobby_state_store_test \
    -pthread \
    tools/gbe_dota_lobby_state_store_test/gbe_dota_lobby_state_store_test.cpp \
    dll/gbe_dota_lobby_state_store.cpp

build_and_run \
    gbe_dota_handler_registry_test \
    tools/gbe_dota_handler_registry_test/gbe_dota_handler_registry_test.cpp

build_and_run \
    gbe_proto_wire_test \
    tools/gbe_proto_wire_test/gbe_proto_wire_test.cpp \
    dll/gbe_proto_wire.cpp \
    dll/gbe_dota_gc_wire.cpp \
    dll/gbe_dota_gc_router.cpp \
    dll/gbe_dota_chat_flow.cpp \
    dll/gbe_dota_lobby_state.cpp \
    dll/gbe_dota_reconnect_context.cpp \
    dll/gbe_dota_lobby_launch_flow.cpp \
    dll/gbe_dota_lobby_member_flow.cpp \
    dll/gbe_dota_lobby_payload_flow.cpp \
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

printf '[run] %s game_flow\n' gc_replay_test
"$BUILD_DIR/gc_replay_test" \
    tools/gc_replay_test/fixtures/game_flow.txt \
    --expect tools/gc_replay_test/fixtures/game_flow.expected.txt

printf '[run] %s cache_and_items\n' gc_replay_test
"$BUILD_DIR/gc_replay_test" \
    tools/gc_replay_test/fixtures/cache_and_items.txt \
    --expect tools/gc_replay_test/fixtures/cache_and_items.expected.txt

if [[ "$FULL" -eq 1 ]]; then
    printf '[run] %s chat_channel\n' gc_replay_test
    "$BUILD_DIR/gc_replay_test" \
        tools/gc_replay_test/fixtures/chat_channel.txt \
        --expect tools/gc_replay_test/fixtures/chat_channel.expected.txt

    printf '[run] %s lobby_lifecycle\n' gc_replay_test
    "$BUILD_DIR/gc_replay_test" \
        tools/gc_replay_test/fixtures/lobby_lifecycle.txt \
        --expect tools/gc_replay_test/fixtures/lobby_lifecycle.expected.txt

    printf '[run] %s wire_edge_cases\n' gc_replay_test
    "$BUILD_DIR/gc_replay_test" \
        tools/gc_replay_test/fixtures/wire_edge_cases.txt \
        --expect tools/gc_replay_test/fixtures/wire_edge_cases.expected.txt

    build_and_run \
        gbe_dota_lobby_flow_test \
        tools/gbe_dota_lobby_flow_test/gbe_dota_lobby_flow_test.cpp \
        dll/gbe_dota_chat_flow.cpp \
        dll/gbe_dota_lobby_launch_flow.cpp \
        dll/gbe_dota_lobby_member_flow.cpp \
        dll/gbe_dota_lobby_payload_flow.cpp \
        dll/gbe_dota_lobby_flow.cpp \
        dll/gbe_dota_lobby_state.cpp \
        dll/gbe_dota_reconnect_context.cpp \
        dll/gbe_dota_lobby_publish.cpp \
        dll/gbe_dota_lobby_snapshot.cpp \
        dll/gbe_dota_custom_game.cpp \
        dll/gbe_dota_gc_wire.cpp \
        dll/gbe_proto_wire.cpp

    build_and_run \
        gbe_dota_lobby_state_test \
        tools/gbe_dota_lobby_state_test/gbe_dota_lobby_state_test.cpp \
        dll/gbe_dota_serialized_connection_state.cpp \
        dll/gbe_dota_lifecycle_actions.cpp \
        dll/gbe_dota_lobby_state.cpp \
        dll/gbe_dota_reconnect_context.cpp \
        dll/gbe_dota_chat_flow.cpp \
        dll/gbe_dota_lobby_launch_flow.cpp \
        dll/gbe_dota_lobby_member_flow.cpp \
        dll/gbe_dota_lobby_payload_flow.cpp \
        dll/gbe_dota_lobby_flow.cpp \
        dll/gbe_dota_custom_game.cpp \
        dll/gbe_dota_gc_wire.cpp \
        dll/gbe_proto_wire.cpp

    build_and_run \
        gbe_dota_custom_game_test \
        tools/gbe_dota_custom_game_test/gbe_dota_custom_game_test.cpp \
        dll/gbe_dota_custom_game.cpp \
        dll/gbe_dota_custom_lobby_http.cpp \
        dll/gbe_proto_wire.cpp
fi

printf '[build] %s\n' gbe_dota_gc_payload_helpers_test
"$CXX_BIN" "${COMMON_FLAGS[@]}" \
    -Itools/gbe_dota_gc_payload_helpers_test \
    -Itools/gbe_dota_gc_payload_helpers_test/pb_stubs \
    tools/gbe_dota_gc_payload_helpers_test/test_wrapper.cpp \
    tools/gbe_dota_gc_payload_helpers_test/gbe_dota_gc_payload_helpers_test.cpp \
    dll/gbe_proto_wire.cpp \
    dll/gbe_gc_message_utils.cpp \
    dll/gbe_dota_chat_flow.cpp \
    dll/gbe_dota_lobby_launch_flow.cpp \
    dll/gbe_dota_lobby_member_flow.cpp \
    dll/gbe_dota_lobby_payload_flow.cpp \
    dll/gbe_dota_lobby_flow.cpp \
    dll/gbe_dota_lobby_state.cpp \
    dll/gbe_dota_lobby_state_store.cpp \
    dll/gbe_dota_reconnect_context.cpp \
    dll/gbe_dota_lobby_publish.cpp \
    dll/gbe_dota_lobby_snapshot.cpp \
    dll/gbe_dota_custom_game.cpp \
    dll/gbe_dota_custom_lobby_http.cpp \
    dll/gbe_dota_gc_wire.cpp \
    dll/gbe_dota_gc_router.cpp \
    -o "$BUILD_DIR/gbe_dota_gc_payload_helpers_test"

printf '[run] %s\n' gbe_dota_gc_payload_helpers_test
"$BUILD_DIR/gbe_dota_gc_payload_helpers_test"

printf '[build] %s\n' gbe_dota_handler_test
"$CXX_BIN" "${COMMON_FLAGS[@]}" \
    -Itools/gbe_dota_handler_test \
    -Itools/gbe_dota_gc_payload_helpers_test/pb_stubs \
    tools/gbe_dota_handler_test/test_wrapper.cpp \
    tools/gbe_dota_handler_test/free_func_stubs.cpp \
    tools/gbe_dota_handler_test/smoke_test.cpp \
    dll/gbe_proto_wire.cpp \
    dll/gbe_gc_message_utils.cpp \
    dll/gbe_dota_chat_flow.cpp \
    dll/gbe_dota_lobby_launch_flow.cpp \
    dll/gbe_dota_lobby_member_flow.cpp \
    dll/gbe_dota_lobby_payload_flow.cpp \
    dll/gbe_dota_lobby_flow.cpp \
    dll/gbe_dota_lobby_state.cpp \
    dll/gbe_dota_reconnect_context.cpp \
    dll/gbe_dota_lobby_publish.cpp \
    dll/gbe_dota_lobby_snapshot.cpp \
    dll/gbe_dota_custom_game.cpp \
    dll/gbe_dota_custom_lobby_http.cpp \
    dll/gbe_dota_gc_wire.cpp \
    dll/gbe_dota_gc_router.cpp \
    -o "$BUILD_DIR/gbe_dota_handler_test"

printf '[run] %s\n' gbe_dota_handler_test
"$BUILD_DIR/gbe_dota_handler_test"

if [[ "$FULL" -eq 1 ]]; then
    printf 'all GC offline tests passed (--full)\n'
else
    printf 'fast GC offline tests passed\n'
fi
