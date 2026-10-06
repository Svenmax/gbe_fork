#!/usr/bin/env python3
"""Phase 2.5 Step 4: Move 10 lobby-snapshot/build helper member functions from
steam_game_coordinator.cpp to a new TU gbe_dota_lobby_snapshot_coordinator.cpp.

Based on the coupling analysis in tools/_phase25_coupling_report.txt and the
externalization done in _phase25_externalize.py:

  - 8 unique target member function names (Steam_Game_Coordinator:: methods),
    2 of which have 2 overloads each => 10 definitions total.
  - The 2 file-scope *Impl static helpers and 2 replay helpers were promoted
    to external linkage in step 3 (List X->Y) because they have transitive
    dependencies on List Z statics that stay in the main file. Their
    DEFINITIONS stay in steam_game_coordinator.cpp; only the 10 member
    function definitions move.
  - The target functions are NON-CONTIGUOUS in the main file (interleaved
    with launch/teardown functions that stay). This script locates each
    definition by name + brace tracking and removes it individually.

Premake5.lua uses `"dll/**"` glob, so the new TU is picked up automatically.
"""
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
NEW_PATH = "/workspace/dll/gbe_dota_lobby_snapshot_coordinator.cpp"

# License + includes header (mirrors gbe_dota_lobby_flow_coordinator.cpp)
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

# 8 unique target member function names (Steam_Game_Coordinator::).
# 2 of them have 2 overloads each, yielding 10 definitions total.
TARGET_FUNCTIONS = [
    "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay",
    "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload",
    "GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate",
    "GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots",
    "GBE_GetDotaGenericLobbySnapshots",
    "GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot",
    "GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed",
    "GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate",
]


def find_all_member_function_starts(lines, func_name):
    """Find ALL line indices where `.*Steam_Game_Coordinator::FUNC_NAME(` begins.
    Returns a list of start indices (handles overloaded functions)."""
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

    # 1. Locate all 10 target member function definitions (handling overloads)
    spans = []  # (start_idx, end_idx, label)
    for fname in TARGET_FUNCTIONS:
        starts = find_all_member_function_starts(lines, fname)
        for s in starts:
            e = find_function_end(lines, s)
            spans.append((s, e, f"{fname}"))

    print(f"Found {len(spans)} target member function definitions")
    if len(spans) != 10:
        print(f"WARNING: expected 10 definitions, found {len(spans)}")

    # Sort spans by start line
    spans.sort(key=lambda s: s[0])

    # Check for overlaps
    for j in range(1, len(spans)):
        if spans[j][0] <= spans[j-1][1]:
            print(f"ERROR: overlapping spans: {spans[j-1]} and {spans[j]}")
            sys.exit(1)

    # 2. Build the new TU content
    new_parts = [FILE_HEADER]
    new_parts.append("// --- Lobby-snapshot/build helper member functions "
                     "(moved from steam_game_coordinator.cpp) ---\n\n")

    for start, end, label in spans:
        chunk = "".join(lines[start:end+1])
        new_parts.append(chunk)
        new_parts.append("\n\n")

    new_content = "".join(new_parts)

    # 3. Build the modified main file: remove extracted spans + trailing blanks
    remove = set()
    for start, end, label in spans:
        for i in range(start, end + 1):
            remove.add(i)
        # Also remove up to 2 trailing blank lines after each span
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

    # 4. Write both files
    with open(NEW_PATH, "w") as f:
        f.write(new_content)
    new_line_count = new_content.count("\n")
    print(f"Wrote {NEW_PATH}: {new_line_count} lines")

    with open(CPP_PATH, "w") as f:
        f.write(modified_content)
    modified_line_count = modified_content.count("\n")
    print(f"Modified {CPP_PATH}: {original_line_count} -> {modified_line_count} lines "
          f"(removed {original_line_count - modified_line_count})")

    # 5. Summary
    extracted_lines = sum(e - s + 1 for s, e, _ in spans)
    print(f"\nExtracted {len(spans)} definitions ({extracted_lines} lines)")
    print("\nSpan details:")
    for start, end, label in spans:
        print(f"  {label:60s}  lines {start+1:5d}-{end+1:5d}  ({end-start+1} lines)")


if __name__ == "__main__":
    main()
