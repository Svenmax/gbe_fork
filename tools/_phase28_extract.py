#!/usr/bin/env python3
"""Phase 2.8 Step 3: Move 5 welcome/hello member functions + 1 List X static
symbol from steam_game_coordinator.cpp to a new TU
gbe_dota_welcome_coordinator.cpp.

Per the coupling analysis in tools/_phase28_coupling_report.txt:

  - 5 target member functions (Steam_Game_Coordinator:: methods), no overloads.
  - List Y = 0 (no externalization needed; earlier phases cover all sharing).
  - List X = 1 static symbol, verified to have NO transitive dependency on
    List Z statics (it is a `static const uint8[]` raw byte template — pure
    data, no function calls). It can move as `static` to the new TU:
      * GBE_kDotaCacheSubscribedTemplate  (const uint8 array, L293)
  - The 5 target functions are NON-CONTIGUOUS (interleaved with
    handle_motd_request, handle_respawn, callback_respawn_request,
    steam_network_callback, initialize_gc, on_client_* which stay). Each is
    located by name + brace tracking and removed individually.

Premake5.lua uses `"dll/**"` glob, so the new TU is picked up automatically.
"""
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
NEW_PATH = "/workspace/dll/gbe_dota_welcome_coordinator.cpp"

FILE_HEADER = """/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>
#include <string>
#include <vector>
#include <unordered_set>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

"""

# 5 target member function names (Steam_Game_Coordinator::). No overloads.
TARGET_FUNCTIONS = [
    "GBE_PatchDotaLoginCacheSubscribedInventory",
    "callback_client_welcome",
    "callback_server_welcome",
    "GBE_MaybePrimeDotaServerWelcomeFromCache",
    "GBE_PushDotaLoginSyncMessages",
]

# List X static symbols to move WITH the target functions (keep `static`).
# Each entry: (name, kind) where kind is "variable" or "function".
# Verified: GBE_kDotaCacheSubscribedTemplate is a `static const uint8[]` raw
# byte template — pure data, no transitive dependency on List Z statics.
LIST_X_STATICS = [
    ("GBE_kDotaCacheSubscribedTemplate", "variable"),
]


def find_all_member_function_starts(lines, func_name):
    """Find ALL line indices where `.*Steam_Game_Coordinator::FUNC_NAME(` begins."""
    pat = re.compile(r'^[a-zA-Z_][\w:&*<>,\s\(\)]*?\bSteam_Game_Coordinator::' +
                     re.escape(func_name) + r'\s*\(')
    starts = []
    for i, line in enumerate(lines):
        if pat.match(line):
            starts.append(i)
    if not starts:
        raise RuntimeError(f"Could not find function start for {func_name}")
    return starts


def find_function_end(lines, start):
    """Track brace depth from signature line to its matching closing brace."""
    depth = 0
    found_open = False
    for i in range(start, len(lines)):
        line = lines[i]
        cleaned = strip_comments_and_strings(line)
        opens = cleaned.count("{")
        closes = cleaned.count("}")
        depth += opens - closes
        if opens > 0:
            found_open = True
        if found_open and depth <= 0:
            return i
    raise RuntimeError(f"Could not find function end starting at line {start+1}")


def find_variable_end(lines, start):
    """Find the next line whose stripped content ends with ';'."""
    for i in range(start, len(lines)):
        if lines[i].rstrip().endswith(";"):
            return i
    raise RuntimeError(f"Could not find terminating ';' for variable at line {start+1}")


def find_static_symbol_span(lines, name, kind):
    """Find the span (start_idx, end_idx) of a file-scope `static` symbol by name.
    Matches `static ... NAME` at column 0. Handles array defs (NAME[])."""
    pat = re.compile(r'^static\s+(?:const\s+|constexpr\s+)*[\w:<>,\s\*\(\)]*?\b' +
                     re.escape(name) + r'\s*[\(\{=;\[]')
    for i, line in enumerate(lines):
        if pat.match(line):
            if kind == "function":
                return i, find_function_end(lines, i)
            else:
                return i, find_variable_end(lines, i)
    raise RuntimeError(f"Could not find static symbol: {name}")


def strip_comments_and_strings(line):
    """Remove string literals and // comments from a single line for brace counting."""
    out = []
    i = 0
    n = len(line)
    in_string = False
    in_char = False
    while i < n:
        c = line[i]
        nxt = line[i+1] if i+1 < n else ""
        if in_string:
            if c == "\\":
                out.append("  ")
                i += 2
                continue
            if c == '"':
                in_string = False
                out.append('"')
                i += 1
                continue
            out.append(" ")
            i += 1
            continue
        if in_char:
            if c == "\\":
                out.append("  ")
                i += 2
                continue
            if c == "'":
                in_char = False
                out.append("'")
                i += 1
                continue
            out.append(" ")
            i += 1
            continue
        if c == "/" and nxt == "/":
            break
        if c == "/" and nxt == "*":
            break
        if c == '"':
            in_string = True
            out.append('"')
            i += 1
            continue
        if c == "'":
            in_char = True
            out.append("'")
            i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def main():
    with open(CPP_PATH, "r") as f:
        lines = f.readlines()

    original_line_count = len(lines)

    # 1. Locate all 5 target member function definitions
    member_spans = []  # (start_idx, end_idx, label)
    for fname in TARGET_FUNCTIONS:
        for s in find_all_member_function_starts(lines, fname):
            e = find_function_end(lines, s)
            member_spans.append((s, e, fname))

    # 2. Locate the 1 List X static symbol
    static_spans = []  # (start_idx, end_idx, label)
    for name, kind in LIST_X_STATICS:
        s, e = find_static_symbol_span(lines, name, kind)
        static_spans.append((s, e, f"static {kind} {name}"))

    all_spans = member_spans + static_spans
    print(f"Found {len(member_spans)} target member functions + {len(static_spans)} List X statics")

    # Sort by start line
    all_spans.sort(key=lambda s: s[0])

    # Check for overlaps
    for j in range(1, len(all_spans)):
        if all_spans[j][0] <= all_spans[j-1][1]:
            print(f"ERROR: overlapping spans: {all_spans[j-1]} and {all_spans[j]}")
            sys.exit(1)

    # 3. Build the new TU content.
    # Order: List X statics first (they're dependencies), then member functions.
    new_parts = [FILE_HEADER]
    new_part_labels = ["FILE_HEADER"]

    for s, e, label in static_spans:
        chunk = "".join(lines[s:e+1])
        new_parts.append(chunk)
        new_part_labels.append(label)
        print(f"  + static  {label:50s}  lines {s+1:5d}-{e+1:5d}  ({e-s+1} lines)")

    for s, e, label in member_spans:
        chunk = "".join(lines[s:e+1])
        new_parts.append(chunk)
        new_part_labels.append(label)
        print(f"  + member  {label:50s}  lines {s+1:5d}-{e+1:5d}  ({e-s+1} lines)")

    new_content = "".join(new_parts)

    # 4. Remove the moved spans from the main file (reverse order to preserve indices).
    # Also remove trailing blank line(s) after each span for cleanliness.
    remove_indices = set()
    for s, e, label in all_spans:
        for i in range(s, e+1):
            remove_indices.add(i)
        # Remove one trailing blank line if present
        if e+1 < len(lines) and lines[e+1].strip() == "":
            remove_indices.add(e+1)

    new_lines = [line for i, line in enumerate(lines) if i not in remove_indices]

    print(f"\nMain file: {original_line_count} -> {len(new_lines)} lines (-{original_line_count - len(new_lines)})")
    print(f"New TU: {new_content.count(chr(10))} lines")

    # 5. Write files
    with open(NEW_PATH, "w") as f:
        f.write(new_content)
    print(f"Wrote {NEW_PATH}")

    with open(CPP_PATH, "w") as f:
        f.writelines(new_lines)
    print(f"Wrote {CPP_PATH}")

    # 6. Summary
    print("\n=== SUMMARY ===")
    print(f"Moved {len(member_spans)} member functions + {len(static_spans)} List X statics")
    print(f"Main file: {original_line_count} -> {len(new_lines)} (-{original_line_count - len(new_lines)} lines)")
    print(f"New TU: {NEW_PATH} ({new_content.count(chr(10))} lines)")


if __name__ == "__main__":
    main()
