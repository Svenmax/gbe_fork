#!/usr/bin/env python3
"""Phase 2.9 Step 4: Move all 27 file-scope static helper symbols (28 defs,
incl. 1 overload pair) from steam_game_coordinator.cpp to a new TU
gbe_dota_gc_payload_helpers.cpp.

Per the coupling analysis in tools/_phase29_coupling_report.txt:

  - 27 unique static symbol names (28 definitions; GBE_ComposeDotaPracticeLobbySOObjects
    has 2 overloads at L65 and L1573).
  - List Y = 15 symbols referenced from main-file member functions OR from
    already-externalized free functions (which stay in main file). These MUST
    have `static` removed and be declared `extern` in gbe_dota_gc_internal.h
    so the main file can still call them.
  - List X = 12 symbols referenced ONLY from other static helpers. They stay
    `static` in the new TU (internal linkage, same-TU visibility is enough
    because all their callers also move to the new TU).
  - List Z = 0, Unused = 0.

Strategy: move ALL 28 definitions to the new TU. For List Y definitions,
strip `static` (and `constexpr`) so they have external linkage. For List X,
keep `static`. Generate extern declarations for List Y symbols and write
them to tools/_phase29_extern_decls.txt for review before inserting into
the header.

Premake5.lua uses `"dll/**"` glob, so the new TU is picked up automatically.
"""
import os
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
NEW_PATH = "/workspace/dll/gbe_dota_gc_payload_helpers.cpp"
EXTERN_DECL_PATH = "/workspace/tools/_phase29_extern_decls.txt"

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

# List Y symbols (referenced from main-file member funcs or externalized funcs).
# These need `static` stripped and an `extern` declaration in the header.
LIST_Y_NAMES = {
    "GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload",
    "GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload",
    "GBE_AdaptDotaTopCustomGamesListPayload",
    "GBE_BuildDirectDotaClientWelcome",
    "GBE_ComposeDotaClientWelcome",
    "GBE_ExtractDirectDotaHelloContext",
    "GBE_ExtractDirectDotaServerHelloContext",
    "GBE_ExtractDotaHelloContext",
    "GBE_ForceDotaLobbyUpdateOwnerSOID",
    "GBE_IsDotaOtherLeftChannelPayloadForChannel",
    "GBE_LogGCProtoBoundary",
    "GBE_PatchDotaPracticeLobbyLaunchTemplate",
    "GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate",
    "GBE_kDotaPracticeLobbyCacheSubscribedTemplate",          # variable (array)
    "GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex", # variable (constexpr)
}


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
    """Find the next line whose stripped content ends with ';'.
    For multi-line array/struct initializers, track to the terminating ';'."""
    for i in range(start, len(lines)):
        if lines[i].rstrip().endswith(";"):
            return i
    raise RuntimeError(f"Could not find terminating ';' for variable at line {start+1}")


def strip_comments_and_strings(line):
    """Remove string literals and // comments from a single line."""
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


def collect_static_symbols(lines):
    """Identify all file-scope `static` definitions (column-0 `static`).
    Returns list of dicts with name, start, end, kind, is_list_y."""
    statics = []
    for i, line in enumerate(lines):
        if not line.startswith("static "):
            continue
        m = re.search(r'\b(GBE_\w+|ser_\w+|deser_\w+)\s*[\(\{=;\[]', line)
        if not m:
            m = re.search(r'\bstatic\s+(?:const\s+|constexpr\s+)*[\w:<>,\s\*\(\)]*?\b(\w+)\s*[\(\{=;\[]', line)
            if not m:
                continue
            name = m.group(1)
        else:
            name = m.group(1)
        if "(" in line:
            kind = "function"
            end = find_function_end(lines, i)
        else:
            kind = "variable"
            end = find_variable_end(lines, i)
        statics.append({
            "name": name,
            "start": i,
            "end": end,
            "kind": kind,
            "is_list_y": name in LIST_Y_NAMES,
            "signature_line": line.rstrip(),
        })
    return statics


def extract_function_signature(lines, start):
    """Extract the function signature from `static ...)` (may span multiple lines).
    Returns the signature string WITHOUT `static ` prefix and WITHOUT trailing `{`."""
    sig_lines = []
    for i in range(start, len(lines)):
        sig_lines.append(lines[i])
        cleaned = strip_comments_and_strings(lines[i])
        if ")" in cleaned:
            break
    sig = "".join(sig_lines)
    # Strip 'static ' prefix
    sig = re.sub(r'^static\s+', '', sig)
    # Truncate at the closing ')' (keep it), drop anything after (e.g. ' {', ' const {')
    m = re.search(r'\)', sig)
    if m:
        sig = sig[:m.end()]
    return sig.strip()


def extract_variable_extern(lines, start):
    """Generate an extern declaration for a static variable.
    Handles `static const TYPE NAME[] = {...};` and `static constexpr const TYPE *NAME = "...";`.
    Returns `extern const TYPE NAME[];` or `extern const TYPE *NAME;`."""
    first = lines[start]
    # Strip 'static ' and 'constexpr '
    s = re.sub(r'^static\s+', '', first)
    s = re.sub(r'^constexpr\s+', '', s)
    # For arrays: `const uint8 NAME[] = {` -> `extern const uint8 NAME[];`
    m_arr = re.match(r'const\s+([\w:<>\s\*]+?)\s+(\w+)\s*\[\s*\]', s)
    if m_arr:
        return f"extern const {m_arr.group(1).strip()} {m_arr.group(2)}[];"
    # For pointer/scalar: `const char *NAME =` or `const TYPE NAME =`
    m_var = re.match(r'const\s+([\w:<>\s\*]+?)\s*\*?\s*(\w+)\s*=', s)
    if m_var:
        # Check if it's a pointer
        if '*' in s[:s.index('=')]:
            return f"extern const {m_var.group(1).strip()} *{m_var.group(2)};"
        return f"extern const {m_var.group(1).strip()} {m_var.group(2)};"
    # Fallback: try generic
    m_generic = re.match(r'([\w:<>\s\*]+?)\s+(\w+)\s*[\[=]', s)
    if m_generic:
        return f"extern {m_generic.group(1).strip()} {m_generic.group(2)};"
    raise RuntimeError(f"Could not extract variable extern from: {first}")


def main():
    with open(CPP_PATH, "r") as f:
        lines = f.readlines()

    original_line_count = len(lines)

    # 1. Collect all static symbols
    statics = collect_static_symbols(lines)
    print(f"Found {len(statics)} static definitions")

    # Sort by start line
    statics.sort(key=lambda s: s["start"])

    # Check for overlaps
    for j in range(1, len(statics)):
        if statics[j]["start"] <= statics[j-1]["end"]:
            print(f"ERROR: overlapping: {statics[j-1]['name']} ({statics[j-1]['start']+1}-{statics[j-1]['end']+1}) and {statics[j]['name']} ({statics[j]['start']+1}-{statics[j]['end']+1})")
            sys.exit(1)

    # 2. Build new TU content: FILE_HEADER + all defs (List Y: strip static/constexpr)
    new_parts = [FILE_HEADER]
    extern_decls = []  # (name, decl) for List Y

    for s in statics:
        chunk_lines = lines[s["start"]:s["end"]+1]
        if s["is_list_y"]:
            # Strip 'static ' from the first line; also 'constexpr ' if present
            first = chunk_lines[0]
            first = re.sub(r'^static\s+', '', first)
            first = re.sub(r'^constexpr\s+', '', first)
            chunk_lines = [first] + chunk_lines[1:]
            # Generate extern declaration
            if s["kind"] == "function":
                sig = extract_function_signature(lines, s["start"])
                decl = f"extern {sig};"
            else:
                decl = extract_variable_extern(lines, s["start"])
            extern_decls.append((s["name"], decl, s["kind"], s["start"]+1))
        new_parts.append("".join(chunk_lines))
        tag = "Y(ext)" if s["is_list_y"] else "X(stc)"
        print(f"  + {tag} {s['kind']:8s} {s['name']:55s}  lines {s['start']+1:5d}-{s['end']+1:5d}  ({s['end']-s['start']+1} lines)")

    new_content = "".join(new_parts)

    # 3. Remove moved spans from main file (+ trailing blank line)
    remove_indices = set()
    for s in statics:
        for i in range(s["start"], s["end"]+1):
            remove_indices.add(i)
        if s["end"]+1 < len(lines) and lines[s["end"]+1].strip() == "":
            remove_indices.add(s["end"]+1)

    new_lines = [line for i, line in enumerate(lines) if i not in remove_indices]

    print(f"\nMain file: {original_line_count} -> {len(new_lines)} lines (-{original_line_count - len(new_lines)})")
    print(f"New TU: {new_content.count(chr(10))} lines")
    print(f"Extern declarations to add: {len(extern_decls)}")

    # 4. Write files
    with open(NEW_PATH, "w") as f:
        f.write(new_content)
    print(f"Wrote {NEW_PATH}")

    with open(CPP_PATH, "w") as f:
        f.writelines(new_lines)
    print(f"Wrote {CPP_PATH}")

    # 5. Write extern declarations for review
    with open(EXTERN_DECL_PATH, "w") as f:
        f.write("// Phase 2.9 extern declarations for gbe_dota_gc_internal.h\n")
        f.write(f"// {len(extern_decls)} symbols (List Y: referenced from main-file staying code)\n\n")
        for name, decl, kind, defline in extern_decls:
            f.write(f"// {kind}, was at main line {defline}\n")
            f.write(decl + "\n\n")
    print(f"Wrote {EXTERN_DECL_PATH} (for review)")

    # 6. Summary
    print("\n=== SUMMARY ===")
    print(f"Moved {len(statics)} static definitions ({len(extern_decls)} externalized, {len(statics)-len(extern_decls)} kept static)")
    print(f"Main file: {original_line_count} -> {len(new_lines)} (-{original_line_count - len(new_lines)} lines)")
    print(f"New TU: {NEW_PATH} ({new_content.count(chr(10))} lines)")


if __name__ == "__main__":
    main()
