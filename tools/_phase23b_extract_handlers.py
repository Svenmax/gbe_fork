#!/usr/bin/env python3
"""Phase 2.3b: Move 62 GBE_HandleDota* handlers + 23 List A static symbols
from steam_game_coordinator.cpp to gbe_dota_handlers.cpp.

List A statics keep their `static` keyword (internal to the new TU).
Handlers are Steam_Game_Coordinator:: member functions.
"""
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
NEW_PATH = "/workspace/dll/gbe_dota_handlers.cpp"

# License + includes header (same as gbe_dota_lobby_state_coordinator.cpp)
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

# List A static symbol names (handler-only, safe to move with `static` intact)
LIST_A_SYMBOLS = [
    "GBE_kSteamGamesPlayedWithDataBlob",
    "GBE_kSteamAuthList",
    "GBE_kDota8730TemplateHex",
    "GBE_kDota8331TemplateHex",
    "GBE_kDotaOfficial8745TemplateHex",
    "GBE_kDota8678Template",
    "GBE_kDota8136Template",
    "GBE_kDota2538Template",
    "GBE_kDota2618Template",
    "GBE_kDota8674Template",
    "GBE_kDota8677Template",
    "GBE_kDota7198Template",
    "GBE_kDota8079Template",
    "GBE_kDota8854Template",
    "GBE_kDota9024Template",
    "GBE_ApplyDotaCustomGameDetailsRequest",
    "GBE_NormalizeDotaCustomGameDetailsFromInstalledMod",
    "GBE_GenerateDotaLobbyId",
    "GBE_GenerateDotaChatChannelId",
    "GBE_GenerateDotaMatchId",
    "GBE_AdaptDotaLobbyInviteCacheSubscribedPayload",
    "GBE_IsDotaLobbyInviteCacheSubscribedPayload",
    "GBE_AdaptDota7034ConnectedPlayersResponsePayload",
]


def find_line_end_function(lines, start_idx):
    """Find the end of a function definition starting at start_idx.
    Track brace depth; function ends when depth returns to 0 after first `{`."""
    depth = 0
    found_open = False
    idx = start_idx
    while idx < len(lines):
        line = lines[idx]
        opens = line.count("{")
        closes = line.count("}")
        depth += opens - closes
        if opens > 0:
            found_open = True
        if found_open and depth <= 0:
            return idx
        idx += 1
    raise ValueError(f"Could not find end of function starting at line {start_idx+1}")


def find_line_end_statement(lines, start_idx):
    """Find the end of a variable/array/string definition.
    Ends at a line whose last non-whitespace char is `;` or `};`."""
    idx = start_idx
    while idx < len(lines):
        stripped = lines[idx].rstrip()
        if stripped.endswith(";"):
            return idx
        idx += 1
    raise ValueError(f"Could not find end of statement starting at line {start_idx+1}")


def main():
    with open(CPP_PATH, "r") as f:
        lines = f.readlines()

    # Collect all spans to extract: (start_idx, end_idx, label)
    spans = []

    # 1. Find handler functions
    handler_re = re.compile(r'^(bool|void|uint\w*|std::\w+|int\w*|size_t)\s+Steam_Game_Coordinator::GBE_HandleDota')
    for i, line in enumerate(lines):
        if handler_re.match(line):
            end = find_line_end_function(lines, i)
            # Extract function name for label
            m = re.search(r'GBE_HandleDota\w+', line)
            name = m.group(0) if m else f"handler@{i+1}"
            spans.append((i, end, f"handler:{name}"))

    print(f"Found {sum(1 for s in spans if s[2].startswith('handler:'))} handler functions")

    # 2. Find List A static symbols
    for sym in LIST_A_SYMBOLS:
        found = False
        for i, line in enumerate(lines):
            # Match `static ... <name>` at column 0
            # Must be a definition, not a usage
            if line.startswith("static ") and re.search(r'\b' + re.escape(sym) + r'\b', line):
                # Determine if it's a function (has `(` in line) or variable
                if "(" in line:
                    end = find_line_end_function(lines, i)
                else:
                    end = find_line_end_statement(lines, i)
                spans.append((i, end, f"static:{sym}"))
                found = True
                break
        if not found:
            print(f"WARNING: List A symbol '{sym}' not found!")
            sys.exit(1)

    print(f"Found {sum(1 for s in spans if s[2].startswith('static:'))} List A static symbols")

    # Sort spans by start line
    spans.sort(key=lambda s: s[0])

    # Check for overlaps
    for j in range(1, len(spans)):
        if spans[j][0] <= spans[j-1][1]:
            print(f"ERROR: overlapping spans: {spans[j-1]} and {spans[j]}")
            sys.exit(1)

    # Build the new file content
    new_parts = [FILE_HEADER]

    # Add a section comment for List A statics
    new_parts.append("// --- List A: handler-only static helpers (moved from steam_game_coordinator.cpp) ---\n\n")

    # Extract spans in original file order, separated by blank lines
    extracted_line_count = 0
    for start, end, label in spans:
        chunk = "".join(lines[start:end+1])
        new_parts.append(chunk)
        new_parts.append("\n\n")  # separator between definitions
        extracted_line_count += (end - start + 1)

    new_content = "".join(new_parts)

    # Build the modified main file: remove extracted spans
    # Mark lines for removal
    remove = set()
    for start, end, label in spans:
        for i in range(start, end + 1):
            remove.add(i)

    # Also remove trailing blank lines immediately after each removed span
    for start, end, label in spans:
        i = end + 1
        while i < len(lines) and lines[i].strip() == "":
            remove.add(i)
            i += 1

    kept_lines = []
    for i, line in enumerate(lines):
        if i not in remove:
            kept_lines.append(line)

    modified_content = "".join(kept_lines)

    # Write both files
    with open(NEW_PATH, "w") as f:
        f.write(new_content)
    print(f"Wrote {NEW_PATH}: {new_content.count(chr(10))} lines")

    with open(CPP_PATH, "w") as f:
        f.write(modified_content)
    print(f"Modified {CPP_PATH}: {modified_content.count(chr(10))} lines (was {len(lines)} lines)")

    # Summary
    print(f"\nExtracted {len(spans)} definitions ({extracted_line_count} lines)")
    print(f"Main file: {len(lines)} -> {modified_content.count(chr(10))} lines")

    # Print span details
    for start, end, label in spans:
        print(f"  {label}: lines {start+1}-{end+1}")


if __name__ == "__main__":
    main()
