#!/usr/bin/env python3
"""Phase 2.11: Coupling analysis + extraction of on_client_connected/disconnected.

Phase 2.11 targets the 2 connection-lifecycle member functions that remain
in steam_game_coordinator.cpp:

  - on_client_connected   (L1902-L1947, 46 lines)
  - on_client_disconnected (L1949-L2002, 54 lines)

Both are Steam_Game_Coordinator:: member functions referencing only:
  - member variables / member functions (visible via class header)
  - GBE_GC_DebugLog, GBE_DescribeDotaLaunchPhase (free fns, declared in
    gbe_dota_gc_internal.h L17/L18)
  - generate_steam_api_call_id (base.h L44)
  - GC_PROFILE_DOTA2 (class enum, steam_game_coordinator.h L84)

=> List X = 0, List Y = 0 (no externalize needed). The 2 functions form a
contiguous block (separated by one blank line) with no leading comment; we
extract them as-is.

This script performs the analysis inline (prints a report) AND does the
extraction in one pass, writing:
  - tools/_phase31_coupling_report.txt
  - dll/gbe_dota_connection_lifecycle.cpp (new TU)
  - dll/steam_game_coordinator.cpp (rewritten without the 2 functions)
"""
import os
import re
import sys
import glob

MAIN_CPP = "/workspace/dll/steam_game_coordinator.cpp"
INTERNAL_H = "/workspace/dll/gbe_dota_gc_internal.h"
CLASS_H = "/workspace/dll/dll/steam_game_coordinator.h"
REPORT_PATH = "/workspace/tools/_phase31_coupling_report.txt"
NEW_TU = "/workspace/dll/gbe_dota_connection_lifecycle.cpp"

TARGET_FNS = [
    "on_client_connected",
    "on_client_disconnected",
]

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


def collect_gbe_refs(body_text):
    return set(re.findall(r"\bGBE_(\w+)\b", body_text))


def is_declared_in_header(name, header_lines):
    full = "GBE_" + name
    pat = re.compile(r"\b" + re.escape(full) + r"\b")
    return any(pat.search(ln) for ln in header_lines)


def is_class_member(name, class_h_lines, split_tus):
    full = "GBE_" + name
    pat = re.compile(r"\b" + re.escape(full) + r"\b")
    if any(pat.search(ln) for ln in class_h_lines):
        return True
    def_re = re.compile(
        r"^[A-Za-z_][\w:&*\s<>,]*\bSteam_Game_Coordinator::" + re.escape(full) + r"\s*\("
    )
    for tu in [MAIN_CPP] + split_tus:
        try:
            tls = read_lines(tu)
        except OSError:
            continue
        if any(def_re.match(ln) for ln in tls):
            return True
    return False


def main():
    main_lines = read_lines(MAIN_CPP)
    header_lines = read_lines(INTERNAL_H)
    class_h_lines = read_lines(CLASS_H)
    split_tus = sorted(glob.glob("/workspace/dll/gbe_dota_*.cpp"))
    orig_count = len(main_lines)

    report = []
    report.append("=" * 78)
    report.append("Phase 2.11 Coupling Analysis: on_client_connected/disconnected")
    report.append("=" * 78)
    report.append("")
    report.append(f"Main file: {MAIN_CPP} ({orig_count} lines)")
    report.append("")

    # Locate ranges.
    ranges = []
    all_refs = set()
    for fn in TARGET_FNS:
        rng = find_member_fn_range(main_lines, fn)
        if rng is None:
            report.append(f"ERROR: not found: {fn}")
            print("\n".join(report))
            sys.exit(1)
        ranges.append((fn, rng))
        s, e = rng
        body = "\n".join(main_lines[s - 1:e])
        refs = collect_gbe_refs(body)
        refs.discard(fn)
        refs = {r for r in refs if "GBE_" + r not in ("GBE_" + t for t in TARGET_FNS)}
        report.append(f"  {fn}: L{s}-L{e} ({e-s+1} lines, GBE_* refs={len(refs)})")
        all_refs |= refs
    report.append("")

    report.append("Referenced GBE_* symbols (excluding self/target set):")
    list_y_candidates = []
    for sym in sorted(all_refs):
        in_header = is_declared_in_header(sym, header_lines)
        is_member = is_class_member(sym, class_h_lines, split_tus)
        tag = []
        if in_header:
            tag.append("HEADER(internal.h)")
        if is_member:
            tag.append("MEMBER(class)")
        status = "+".join(tag) if tag else "?? NOT FOUND"
        report.append(f"  GBE_{sym}: {status}")
        if not in_header and not is_member:
            list_y_candidates.append(sym)
    report.append("")

    report.append("Classification:")
    report.append("  List X (file-scope statics traveling): 0")
    report.append(f"  List Y (need new extern declaration): {len(list_y_candidates)}")
    for sym in list_y_candidates:
        report.append(f"    - GBE_{sym}")
    report.append("")

    if list_y_candidates:
        with open(REPORT_PATH, "w", encoding="utf-8") as f:
            f.write("\n".join(report) + "\n")
        print("List Y not empty, aborting:")
        print("\n".join(report))
        sys.exit(1)

    # Contiguity check.
    for k in range(1, len(ranges)):
        prev_end = ranges[k - 1][1][1]
        cur_start = ranges[k][1][0]
        gap = main_lines[prev_end:cur_start - 1]
        for g in gap:
            gs = g.strip()
            if gs and not gs.startswith("//"):
                report.append(f"ERROR: non-comment in gap: {g!r}")
                with open(REPORT_PATH, "w", encoding="utf-8") as f:
                    f.write("\n".join(report) + "\n")
                print("\n".join(report))
                sys.exit(1)
    report.append("Contiguity check: OK")
    report.append("")

    first_sig = ranges[0][1][0]
    last_end = ranges[-1][1][1]
    # No leading comment for these two (verified via read). Use first_sig.
    block_start = first_sig
    report.append(f"Extraction block: L{block_start}-L{last_end} "
                  f"({last_end - block_start + 1} lines)")

    extracted = main_lines[block_start - 1:last_end]

    new_tu_text = HEADER_BLOCK.rstrip("\n") + "\n\n" + "\n".join(extracted) + "\n"

    before = main_lines[:block_start - 1]
    after = main_lines[last_end:]
    while after and after[0].strip() == "":
        after.pop(0)
    new_main = before + [""] + after

    with open(NEW_TU, "w", encoding="utf-8") as f:
        f.write(new_tu_text)
    with open(MAIN_CPP, "w", encoding="utf-8") as f:
        f.write("\n".join(new_main) + "\n")
    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")

    # Verify.
    new_tu_lines = new_tu_text.splitlines()
    found = 0
    for fn in TARGET_FNS:
        pat = re.compile(r"\bSteam_Game_Coordinator::" + re.escape(fn) + r"\s*\(")
        if any(pat.search(ln) for ln in new_tu_lines):
            found += 1
    remaining = 0
    for fn in TARGET_FNS:
        pat = re.compile(r"^[A-Za-z_][\w:&*\s<>,]*\bSteam_Game_Coordinator::"
                         + re.escape(fn) + r"\s*\(")
        if any(pat.match(ln) for ln in new_main):
            remaining += 1

    report.append("")
    report.append(f"Original main file: {orig_count} lines")
    report.append(f"New main file:      {len(new_main)} lines "
                  f"(delta {len(new_main) - orig_count})")
    report.append(f"New TU:             {len(new_tu_lines)} lines")
    report.append(f"Definitions moved to new TU: {found}/{len(TARGET_FNS)}")
    report.append(f"Definitions remaining in main: {remaining} (expected 0)")

    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report[-10:]))
    if found != len(TARGET_FNS) or remaining != 0:
        print("ERROR: verification failed", file=sys.stderr)
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
