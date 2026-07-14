#!/usr/bin/env python3
"""Phase 2.4b Step 3: Move 10 lobby-flow helper member functions + 4 List X
static symbols from steam_game_coordinator.cpp to a new TU
gbe_dota_lobby_flow_coordinator.cpp.

List X statics keep their `static` keyword (internal to the new TU).
The moved member functions are Steam_Game_Coordinator:: methods; they
remain part of the class via the header declaration.

Based on the coupling analysis in tools/_phase24b_coupling_report.txt:
  - List Y is empty (no externalization needed)
  - List X = 4 constexpr `GBE_kDotaLaunchPersonaState*Hex` constants
  - All other cross-TU dependencies already externalized in Phase 2.2/2.3a/2.4a
"""
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
NEW_PATH = "/workspace/dll/gbe_dota_lobby_flow_coordinator.cpp"

# License + includes header (mirrors gbe_dota_handlers.cpp for consistency)
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

# 10 target lobby-flow helper member functions (Steam_Game_Coordinator::)
TARGET_FUNCTIONS = [
    "GBE_ResetDotaPracticeLobbyLaunchPeripheralState",
    "GBE_ShouldTrackDotaPracticeLobbyLateSteamChain",
    "GBE_MaybeQueueDotaPracticeLobbySteamAuthAck",
    "GBE_UpdateDotaPracticeLobbyLaunchRichPresence",
    "GBE_ClearDotaPracticeLobbyLaunchRichPresence",
    "GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback",
    "GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState",
    "GBE_ReapplyDotaPracticeLobbyLaunchRichPresence",
    "GBE_FinalizeDotaAbandonAfterOtherLeftChannel",
    "GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed",
]

# 4 List X static symbols (referenced only from target functions)
LIST_X_SYMBOLS = [
    "GBE_kDotaLaunchPersonaStateInitServerSetupHex",
    "GBE_kDotaLaunchPersonaStateFindingMatchServerSetupHex",
    "GBE_kDotaLaunchPersonaStateFindingMatchRunHex",
    "GBE_kDotaLaunchPersonaStatePrivateLobbyRunHex",
]


def find_line_end_function(lines, start_idx):
    """Find the end of a function definition starting at start_idx.
    Track brace depth (with string/comment stripping); function ends when
    depth returns to 0 after the first `{`."""
    depth = 0
    found_open = False
    idx = start_idx
    while idx < len(lines):
        line = lines[idx]
        cleaned = strip_comments_and_strings(line)
        opens = cleaned.count("{")
        closes = cleaned.count("}")
        depth += opens - closes
        if opens > 0:
            found_open = True
        if found_open and depth <= 0:
            return idx
        idx += 1
    raise ValueError(f"Could not find end of function starting at line {start_idx+1}")


def find_line_end_statement(lines, start_idx):
    """Find the end of a variable/array/string definition.
    Ends at a line whose last non-whitespace char is `;`."""
    idx = start_idx
    while idx < len(lines):
        stripped = lines[idx].rstrip()
        if stripped.endswith(";"):
            return idx
        idx += 1
    raise ValueError(f"Could not find end of statement starting at line {start_idx+1}")


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

    spans = []  # (start_idx, end_idx, label)

    # 1. Find target member functions
    for fname in TARGET_FUNCTIONS:
        # Match any return-type prefix followed by Steam_Game_Coordinator::FUNC_NAME(
        pat = re.compile(r'^[a-zA-Z_][\w:&*<>,\s\(\)]*?\bSteam_Game_Coordinator::' +
                         re.escape(fname) + r'\s*\(')
        found = False
        for i, line in enumerate(lines):
            if pat.match(line):
                end = find_line_end_function(lines, i)
                spans.append((i, end, f"function:{fname}"))
                found = True
                break
        if not found:
            print(f"ERROR: could not find function {fname}")
            sys.exit(1)

    print(f"Found {len(spans)} target member functions")

    # 2. Find List X static symbols
    for sym in LIST_X_SYMBOLS:
        sym_pat = re.compile(r'\b' + re.escape(sym) + r'\b')
        found = False
        for i, line in enumerate(lines):
            if not line.startswith("static "):
                continue
            if not sym_pat.search(line):
                continue
            # These are all constexpr const char* variables (one-line definitions)
            if "(" in line and not line.rstrip().endswith(";"):
                # Could be a function — use function-end tracker
                end = find_line_end_function(lines, i)
            else:
                end = find_line_end_statement(lines, i)
            spans.append((i, end, f"static:{sym}"))
            found = True
            break
        if not found:
            print(f"ERROR: could not find List X static symbol {sym}")
            sys.exit(1)

    print(f"Found {sum(1 for s in spans if s[2].startswith('static:'))} List X static symbols")

    # Sort spans by start line
    spans.sort(key=lambda s: s[0])

    # Check for overlaps
    for j in range(1, len(spans)):
        if spans[j][0] <= spans[j-1][1]:
            print(f"ERROR: overlapping spans: {spans[j-1]} and {spans[j]}")
            sys.exit(1)

    # Build the new file content
    new_parts = [FILE_HEADER]
    new_parts.append("// --- List X: lobby-flow-only static constants (moved from steam_game_coordinator.cpp) ---\n\n")

    # Extract List X statics first (so they appear before the functions that use them)
    static_spans = [s for s in spans if s[2].startswith("static:")]
    func_spans = [s for s in spans if s[2].startswith("function:")]

    for start, end, label in static_spans:
        chunk = "".join(lines[start:end+1])
        new_parts.append(chunk)
        new_parts.append("\n\n")

    new_parts.append("// --- Lobby-flow helper member functions (moved from steam_game_coordinator.cpp) ---\n\n")

    for start, end, label in func_spans:
        chunk = "".join(lines[start:end+1])
        new_parts.append(chunk)
        new_parts.append("\n\n")

    new_content = "".join(new_parts)

    # Build the modified main file: remove extracted spans + trailing blank lines
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

    # Write both files
    with open(NEW_PATH, "w") as f:
        f.write(new_content)
    new_line_count = new_content.count("\n")
    print(f"Wrote {NEW_PATH}: {new_line_count} lines")

    with open(CPP_PATH, "w") as f:
        f.write(modified_content)
    modified_line_count = modified_content.count("\n")
    original_line_count = len(lines)
    print(f"Modified {CPP_PATH}: {original_line_count} -> {modified_line_count} lines "
          f"(removed {original_line_count - modified_line_count})")

    # Summary
    extracted_lines = sum(e - s + 1 for s, e, _ in spans)
    print(f"\nExtracted {len(spans)} definitions ({extracted_lines} lines)")
    print("\nSpan details:")
    for start, end, label in spans:
        print(f"  {label:55s}  lines {start+1:5d}-{end+1:5d}  ({end-start+1} lines)")


if __name__ == "__main__":
    main()
