#!/usr/bin/env python3
"""Phase 2.10 Step 2: Extract 6 network_callback_* member functions to a new TU.

Moves the following Steam_Game_Coordinator:: member functions from
steam_game_coordinator.cpp to a new gbe_dota_network_callbacks.cpp:

  - network_callback_inventory_request
  - network_callback_inventory_response
  - network_callback_item_update
  - network_callback_item_deletion
  - network_callback_respawn_request
  - network_callback  (top-level dispatcher)

Coupling analysis (see _phase30_analyze.py / _phase30_coupling_report.txt)
confirmed List X = 0 and List Y = 0: every referenced symbol is already
visible through existing headers (class declaration in
dll/dll/steam_game_coordinator.h, extern declarations in
gbe_dota_gc_internal.h, and standard headers like base.h / econ_item.h /
dll.h). No new extern declarations are needed.

The 6 functions form a contiguous block (with leading comments and blank
separators). The whole block is moved as-is to the new TU. The new TU uses
the same license + include block as gbe_dota_inventory_coordinator.cpp
(already proven to compile with the same dependency surface).
"""
import os
import re
import sys

MAIN_CPP = "/workspace/dll/steam_game_coordinator.cpp"
NEW_TU = "/workspace/dll/gbe_dota_network_callbacks.cpp"

TARGET_FNS = [
    "network_callback_inventory_request",
    "network_callback_inventory_response",
    "network_callback_item_update",
    "network_callback_item_deletion",
    "network_callback_respawn_request",
    "network_callback",
]

# License + include block (same as gbe_dota_inventory_coordinator.cpp).
HEADER_BLOCK = """/* Copyright (C) 2019 Mr Goldberg
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


def read_lines(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read().splitlines(keepends=False)


def find_member_fn_range(lines, fn_name):
    """Return (start_line, end_line) 1-indexed inclusive of the definition of
    `... Steam_Game_Coordinator::<fn_name>(...) { ... }`."""
    sig_re = re.compile(
        r"^[A-Za-z_][\w:&*\s<>,]*\bSteam_Game_Coordinator::" + re.escape(fn_name) + r"\s*\("
    )
    n = len(lines)
    for i in range(n):
        if sig_re.match(lines[i]):
            depth = 0
            started = False
            for j in range(i, n):
                for ch in lines[j]:
                    if ch == "{":
                        depth += 1
                        started = True
                    elif ch == "}":
                        depth -= 1
                        if started and depth == 0:
                            return (i + 1, j + 1)
            break
    return None


def find_leading_comment_start(lines, sig_line):
    """Given the 1-indexed signature line, walk upward to include contiguous
    `//` comment lines directly above (skipping blanks). Returns the 1-indexed
    line of the first comment line, or sig_line if no comment precedes."""
    i = sig_line - 2  # 0-indexed line above signature
    while i >= 0:
        stripped = lines[i].lstrip()
        if stripped.startswith("//"):
            i -= 1
            continue
        break
    # i is now the line ABOVE the first comment (or above sig if no comment).
    first_comment = i + 2  # 1-indexed
    if first_comment >= sig_line:
        return sig_line
    # Verify it's actually a comment
    if lines[first_comment - 1].lstrip().startswith("//"):
        return first_comment
    return sig_line


def main():
    lines = read_lines(MAIN_CPP)
    orig_count = len(lines)

    # Locate each function.
    ranges = []
    for fn in TARGET_FNS:
        rng = find_member_fn_range(lines, fn)
        if rng is None:
            print(f"ERROR: could not find definition of {fn}", file=sys.stderr)
            sys.exit(1)
        ranges.append((fn, rng))
        print(f"  {fn}: L{rng[0]}-L{rng[1]} ({rng[1]-rng[0]+1} lines)")

    # Verify contiguity: each function's start should be > previous end and
    # the gap should contain only comments/blank lines.
    for k in range(1, len(ranges)):
        prev_end = ranges[k - 1][1][1]
        cur_start = ranges[k][1][0]
        gap = lines[prev_end:cur_start - 1]  # 0-indexed slice between
        for g in gap:
            gs = g.strip()
            if gs and not gs.startswith("//"):
                print(f"ERROR: non-comment/non-blank line in gap between "
                      f"{ranges[k-1][0]} and {ranges[k][0]}: {g!r}",
                      file=sys.stderr)
                sys.exit(1)
    print("Contiguity check: OK (gaps contain only comments/blanks)")

    first_sig = ranges[0][1][0]
    last_end = ranges[-1][1][1]

    # Include leading comment block above the first function.
    block_start = find_leading_comment_start(lines, first_sig)
    print(f"Extraction block: L{block_start}-L{last_end} "
          f"({last_end - block_start + 1} lines, includes leading comments)")

    # Extract the block (1-indexed inclusive -> 0-indexed slice).
    extracted = lines[block_start - 1:last_end]

    # Build the new TU.
    new_tu_text = HEADER_BLOCK.rstrip("\n") + "\n\n" + "\n".join(extracted) + "\n"

    # Build the new main file: keep everything before block_start, and
    # everything after last_end. Preserve a single blank separator.
    before = lines[:block_start - 1]
    after = lines[last_end:]  # 0-indexed: from last_end onward
    # Trim leading blanks from `after` to avoid stacking blanks, then add one.
    while after and after[0].strip() == "":
        after.pop(0)
    new_main = before + [""] + after

    # Write files.
    with open(NEW_TU, "w", encoding="utf-8") as f:
        f.write(new_tu_text)
    with open(MAIN_CPP, "w", encoding="utf-8") as f:
        f.write("\n".join(new_main) + "\n")

    # Verify: count definitions in new TU.
    new_tu_lines = new_tu_text.splitlines()
    found = 0
    for fn in TARGET_FNS:
        pat = re.compile(r"\bSteam_Game_Coordinator::" + re.escape(fn) + r"\s*\(")
        if any(pat.search(ln) for ln in new_tu_lines):
            found += 1
    # Verify: none remain in main file.
    remaining = 0
    for fn in TARGET_FNS:
        pat = re.compile(r"^[A-Za-z_][\w:&*\s<>,]*\bSteam_Game_Coordinator::"
                         + re.escape(fn) + r"\s*\(")
        if any(pat.match(ln) for ln in new_main):
            remaining += 1

    print("")
    print(f"Original main file: {orig_count} lines")
    print(f"New main file:      {len(new_main)} lines "
          f"(delta {len(new_main) - orig_count})")
    print(f"New TU:             {len(new_tu_lines)} lines")
    print(f"Definitions moved to new TU: {found}/{len(TARGET_FNS)}")
    print(f"Definitions remaining in main: {remaining} (expected 0)")
    if found != len(TARGET_FNS) or remaining != 0:
        print("ERROR: verification failed", file=sys.stderr)
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
