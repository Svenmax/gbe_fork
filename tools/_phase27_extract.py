#!/usr/bin/env python3
"""Phase 2.7 Step 4: Move 20 inventory/item member functions + 1 List X static
symbol from steam_game_coordinator.cpp to a new TU gbe_dota_inventory_coordinator.cpp.

Per the coupling analysis in tools/_phase27_coupling_report.txt:

  - 20 target member functions (Steam_Game_Coordinator:: methods), no overloads.
  - List Y = 1 (deser_var template — already externalized to header in step 3).
  - List X = 1 static symbol, verified to have NO transitive dependency on
    List Z statics (only calls ser_var<uint16> which is an inline template in
    the header, plus std::string::append). It can move as `static` to the
    new TU:
      * ser_varstring  (function, ~L818)
  - The 20 target functions are NON-CONTIGUOUS (interleaved with handle_motd_request,
    handle_respawn, callback_respawn_request, on_client_connected/disconnected
    which stay). Each is located by name + brace tracking and removed individually.

Premake5.lua uses `"dll/**"` glob, so the new TU is picked up automatically.
"""
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
NEW_PATH = "/workspace/dll/gbe_dota_inventory_coordinator.cpp"

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
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

"""

# 20 target member function names (Steam_Game_Coordinator::). No overloads.
TARGET_FUNCTIONS = [
    "item_id_local_to_network",
    "item_id_network_to_local",
    "item_to_gcstruct",
    "item_to_gcprotobuf",
    "handle_set_item_pos",
    "handle_delete_item",
    "handle_set_item_style",
    "handle_adjust_equip_state",
    "handle_set_multiple_item_pos",
    "callback_items_received",
    "callback_items_removed",
    "callback_item_updated",
    "callback_item_deleted",
    "load_items_from_file",
    "save_items_to_file",
    "set_item_pos",
    "delete_item",
    "request_user_items",
    "find_items_request",
    "remove_user_items",
]

# List X static symbols to move WITH the target functions (keep `static`).
# Verified: ser_varstring only calls ser_var<uint16> (inline template in header)
# and std::string::append — no List Z transitive dependency.
LIST_X_STATICS = [
    ("ser_varstring", "function"),
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
    Matches `static ... NAME` at column 0."""
    pat = re.compile(r'^static\s+(?:const\s+|constexpr\s+)*[\w:<>,\s\*\(\)]*?\b' +
                     re.escape(name) + r'\s*[\(\{=;]')
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

    # 1. Locate all 20 target member function definitions
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
    new_parts.append("// --- List X: inventory/item-only static symbols "
                     "(moved from steam_game_coordinator.cpp) ---\n\n")
    for s, e, label in static_spans:
        chunk = "".join(lines[s:e+1])
        new_parts.append(chunk)
        new_parts.append("\n\n")

    new_parts.append("// --- Inventory/item member functions "
                     "(moved from steam_game_coordinator.cpp) ---\n\n")
    for s, e, label in member_spans:
        chunk = "".join(lines[s:e+1])
        new_parts.append(chunk)
        new_parts.append("\n\n")

    new_content = "".join(new_parts)

    # 4. Build the modified main file: remove extracted spans + trailing blanks
    remove = set()
    for start, end, label in all_spans:
        for i in range(start, end + 1):
            remove.add(i)
        # Remove up to 2 trailing blank lines after each span
        i = end + 1
        consumed = 0
        while i < len(lines) and lines[i].strip() == "" and consumed < 2:
            remove.add(i)
            i += 1
            consumed += 1

    kept_lines = []
    for i, line in enumerate(lines):
        if i not in remove:
            kept_lines.append(line)

    modified_content = "".join(kept_lines)

    # 5. Write both files
    with open(NEW_PATH, "w") as f:
        f.write(new_content)
    new_line_count = new_content.count("\n")
    print(f"Wrote {NEW_PATH}: {new_line_count} lines")

    with open(CPP_PATH, "w") as f:
        f.write(modified_content)
    modified_line_count = modified_content.count("\n")
    print(f"Modified {CPP_PATH}: {original_line_count} -> {modified_line_count} lines "
          f"(removed {original_line_count - modified_line_count})")

    # 6. Summary
    member_lines = sum(e - s + 1 for s, e, _ in member_spans)
    static_lines = sum(e - s + 1 for s, e, _ in static_spans)
    print(f"\nExtracted {len(member_spans)} member definitions ({member_lines} lines) + "
          f"{len(static_spans)} List X statics ({static_lines} lines)")
    print("\nSpan details:")
    for start, end, label in all_spans:
        print(f"  {label:60s}  lines {start+1:5d}-{end+1:5d}  ({end-start+1} lines)")


if __name__ == "__main__":
    main()
