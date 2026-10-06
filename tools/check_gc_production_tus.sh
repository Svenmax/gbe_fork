#!/usr/bin/env bash
# Syntax-check every production GC translation unit against real headers.
#
# Closes the "not directly offline-buildable" blind spot: offline tests compile
# handler TUs through stub wrappers, so production include graphs are never
# exercised until a full premake build. This script runs g++ -fsyntax-only on
# each production GC TU with generated protobuf headers, catching missing
# includes, signature drift, and ODR-visible declaration mismatches early.
#
# Prerequisites (installed once per environment):
#   protoc, libprotobuf-dev, libcurl4-openssl-dev, libmbedtls-dev,
#   libopus-dev, portaudio19-dev
#
# Usage:
#   bash tools/check_gc_production_tus.sh [--jobs N] [--keep-going]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEN_DIR="${GC_TU_CHECK_GEN_DIR:-/tmp/opencode/gc_tu_check}"
JOBS=4
KEEP_GOING=0

while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --jobs)
            shift
            JOBS="$1"
            ;;
        --keep-going)
            KEEP_GOING=1
            ;;
        *)
            printf 'usage: %s [--jobs N] [--keep-going]\n' "$(basename "$0")" >&2
            exit 2
            ;;
    esac
    shift
done

cd "$ROOT_DIR"

command -v protoc >/dev/null || { echo "protoc not found; install protobuf-compiler" >&2; exit 1; }
command -v g++ >/dev/null || { echo "g++ not found" >&2; exit 1; }

# --- Generate protobuf headers (idempotent, cached by proto mtime) ---
PROTO_DIR="$GEN_DIR/proto_gen"
mkdir -p "$PROTO_DIR/tf2"
if [[ ! -f "$PROTO_DIR/net.pb.h" || dll/net.proto -nt "$PROTO_DIR/net.pb.h" ]]; then
    protoc dll/net.proto -I./dll --cpp_out="$PROTO_DIR" 2>/dev/null
fi
if [[ ! -f "$PROTO_DIR/steammessages.pb.h" || dll/gc_steam/steammessages.proto -nt "$PROTO_DIR/steammessages.pb.h" ]]; then
    protoc dll/gc_steam/steammessages.proto -I./dll/gc_steam --cpp_out="$PROTO_DIR" 2>/dev/null
fi
if [[ ! -f "$PROTO_DIR/tf2/tf_gcmessages.pb.h" ]]; then
    protoc dll/gc_tf2/*.proto -I./dll/gc_steam -I./dll/gc_tf2 --cpp_out="$PROTO_DIR/tf2" 2>/dev/null
fi

# --- Extract third-party header-only deps that are vendored as tarballs ---
SSQ_INC="$GEN_DIR/libssq/include"
if [[ ! -d "$SSQ_INC" ]]; then
    mkdir -p "$GEN_DIR/ssq_unpack"
    tar xzf third-party/deps/common/libssq/libssq.tar.gz -C "$GEN_DIR/ssq_unpack"
    mkdir -p "$GEN_DIR/libssq"
    cp -r "$GEN_DIR/ssq_unpack/libssq/include" "$GEN_DIR/libssq/"
fi

INCLUDES=(
    -Idll
    -I.
    -Ihelpers
    -Icrash_printer
    -Ilibs
    -Ilibs/utfcpp
    -Isdk
    -Ioverlay_experimental
    -I"$PROTO_DIR"
    -I"$PROTO_DIR/tf2"
    -I"$SSQ_INC"
)

# Mirror the premake Linux production defines (premake5.lua:218,655).
DEFINES=(
    -DUTF_CPP_CPLUSPLUS=201703L
    -DCURL_STATICLIB
    -DCONTROLLER_SUPPORT
    -DEMU_BUILD_STRING=manual
    -DGNUC
    -DNDEBUG
    -DEMU_RELEASE_BUILD
)

# --- Collect production GC TUs ---
mapfile -t TUS < <(ls dll/gbe_dota_*.cpp dll/steam_game_coordinator.cpp dll/gbe_gc_message_utils.cpp dll/gbe_proto_wire.cpp 2>/dev/null)

echo "Checking ${#TUS[@]} production GC translation units..."

FAIL_LIST="$GEN_DIR/failures.txt"
: > "$FAIL_LIST"

check_one() {
    local tu="$1"
    local log
    log="$(mktemp "$GEN_DIR/log.XXXXXX")"
    if g++ -std=c++17 -fsyntax-only -Wno-invalid-offsetof "${INCLUDES[@]}" "${DEFINES[@]}" "$tu" >"$log" 2>&1; then
        rm -f "$log"
        printf '  [ok]   %s\n' "$tu"
    else
        printf '  [FAIL] %s\n' "$tu"
        sed 's/^/         /' "$log" | head -12
        echo "$tu" >> "$FAIL_LIST"
        rm -f "$log"
        return 1
    fi
}

export -f check_one
export GEN_DIR FAIL_LIST
export INCLUDES_SERIALIZED="${INCLUDES[*]}"

rc=0
running=0
for tu in "${TUS[@]}"; do
    check_one "$tu" &
    running=$((running + 1))
    if [[ "$running" -ge "$JOBS" ]]; then
        wait -n || rc=1
        running=$((running - 1))
        if [[ "$rc" -ne 0 && "$KEEP_GOING" -eq 0 ]]; then
            wait || true
            break
        fi
    fi
done
wait || rc=1

fail_count=$(wc -l < "$FAIL_LIST" | tr -d ' ')
if [[ "$fail_count" -gt 0 ]]; then
    echo "FAILED: $fail_count / ${#TUS[@]} production GC TUs did not pass syntax check:"
    sed 's/^/  /' "$FAIL_LIST"
    exit 1
fi

echo "All ${#TUS[@]} production GC TUs passed syntax check."
