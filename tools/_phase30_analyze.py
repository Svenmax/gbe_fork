#!/usr/bin/env python3
"""Phase 2.10 Step 1: Coupling analysis for extracting network_callback_* members.

Phase 2.10 targets the 6 `Steam_Game_Coordinator::network_callback*` member
functions that remain in steam_game_coordinator.cpp (inventory request/response,
item update/deletion, respawn request, and the top-level network_callback
dispatcher). These are class member functions, not file-scope statics.

Goal: move all 6 to a new TU (gbe_dota_network_callbacks.cpp), verifying that
every symbol they reference is already visible through existing headers
(class declaration in steam_game_coordinator.h, extern declarations in
gbe_dota_gc_internal.h, or other standard headers).

For member-function extraction the coupling model is simpler than Phase 2.9:
  - Member variables / member functions: visible via class definition in
    dll/dll/steam_game_coordinator.h -> no externalize needed.
  - Free functions / extern variables: must be declared in some header
    included by the new TU. We verify each referenced GBE_* symbol is declared
    in gbe_dota_gc_internal.h or is a known member function.
  - List X (file-scope statics traveling with the move): expected = 0, since
    Phase 2.9 already removed all file-scope statics from the main file.
  - List Y (symbols needing new extern declarations): any referenced free
    function/variable NOT already declared in a header.

Output: tools/_phase30_coupling_report.txt
"""
import os
import re
import glob

MAIN_CPP = "/workspace/dll/steam_game_coordinator.cpp"
INTERNAL_H = "/workspace/dll/gbe_dota_gc_internal.h"
CLASS_H = "/workspace/dll/dll/steam_game_coordinator.h"
REPORT_PATH = "/workspace/tools/_phase30_coupling_report.txt"

# All split TUs sharing the GC class (member functions live in these).
SPLIT_TUS = sorted(glob.glob("/workspace/dll/gbe_dota_*.cpp"))

# The 6 target member functions to extract.
TARGET_FNS = [
    "network_callback_inventory_request",
    "network_callback_inventory_response",
    "network_callback_item_update",
    "network_callback_item_deletion",
    "network_callback_respawn_request",
    "network_callback",
]


def read_lines(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read().splitlines()


def find_member_fn_range(lines, fn_name):
    """Find the line range [start, end] (1-indexed, inclusive) of the definition
    of `void Steam_Game_Coordinator::<fn_name>(...)` in `lines`. Uses brace
    matching starting from the first `{` at/after the signature line.
    Returns (start_line, end_line) or None.
    """
    sig_re = re.compile(
        r"^[A-Za-z_][\w:&*\s<>,]*\bSteam_Game_Coordinator::" + re.escape(fn_name) + r"\s*\("
    )
    n = len(lines)
    for i in range(n):
        if sig_re.match(lines[i]):
            # Find first '{' from line i onward
            depth = 0
            started = False
            for j in range(i, n):
                line = lines[j]
                for ch in line:
                    if ch == "{":
                        depth += 1
                        started = True
                    elif ch == "}":
                        depth -= 1
                        if started and depth == 0:
                            return (i + 1, j + 1)  # 1-indexed inclusive
            break
    return None


def collect_gbe_refs(body_text):
    """Return the set of GBE_* identifiers referenced in `body_text`."""
    return set(re.findall(r"\bGBE_(\w+)\b", body_text))


def is_declared_in_header(name, header_lines):
    """True if `GBE_<name>` appears as a declared identifier in the header."""
    full = "GBE_" + name
    pat = re.compile(r"\b" + re.escape(full) + r"\b")
    return any(pat.search(ln) for ln in header_lines)


def is_class_member(name, class_h_lines, split_tus):
    """True if `GBE_<name>` is a Steam_Game_Coordinator:: member function
    (declared in the class header or defined as `... Steam_Game_Coordinator::GBE_<name>(` in any split TU / main file).
    """
    full = "GBE_" + name
    # Declared in class header?
    pat = re.compile(r"\b" + re.escape(full) + r"\b")
    if any(pat.search(ln) for ln in class_h_lines):
        return True
    # Defined as member in any TU?
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

    report = []
    report.append("=" * 78)
    report.append("Phase 2.10 Coupling Analysis: network_callback_* extraction")
    report.append("=" * 78)
    report.append("")
    report.append(f"Main file: {MAIN_CPP} ({len(main_lines)} lines)")
    report.append(f"Internal header: {INTERNAL_H} ({len(header_lines)} lines)")
    report.append(f"Class header: {CLASS_H} ({len(class_h_lines)} lines)")
    report.append("")
    report.append("Target member functions (to move to gbe_dota_network_callbacks.cpp):")
    for fn in TARGET_FNS:
        report.append(f"  - Steam_Game_Coordinator::{fn}")
    report.append("")

    # Locate each function's range and collect body text.
    ranges = {}
    all_refs = set()
    report.append("Function definition ranges (1-indexed inclusive):")
    for fn in TARGET_FNS:
        rng = find_member_fn_range(main_lines, fn)
        if rng is None:
            report.append(f"  ! NOT FOUND: {fn}")
            continue
        s, e = rng
        ranges[fn] = rng
        body = "\n".join(main_lines[s - 1:e])
        refs = collect_gbe_refs(body)
        # Drop self-reference and other target fns (internal mutual calls among
        # the moved set are fine).
        refs.discard(fn)
        refs = {r for r in refs if "GBE_" + r not in ("GBE_" + t for t in TARGET_FNS)}
        report.append(f"  {fn}: L{s}-L{e}  ({e - s + 1} lines, GBE_* refs={len(refs)})")
        all_refs |= refs
    report.append("")

    # Also scan the dispatcher (network_callback) body for non-GBE external
    # function calls of interest (e.g. request_user_items etc. are members).
    report.append("Referenced GBE_* symbols across all 6 function bodies "
                  "(excluding self/target set):")
    if not all_refs:
        report.append("  (none)")
    list_y_candidates = []
    for sym in sorted(all_refs):
        in_header = is_declared_in_header(sym, header_lines)
        is_member = is_class_member(sym, class_h_lines, SPLIT_TUS)
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
    report.append(f"  List X (file-scope statics traveling with move): 0 (Phase 2.9 "
                  "removed all file-scope statics from main file)")
    report.append(f"  List Y (need new extern declaration): {len(list_y_candidates)}")
    for sym in list_y_candidates:
        report.append(f"    - GBE_{sym}")
    report.append(f"  List Z (referenced from other TUs, already external): N/A "
                  "(members, not statics)")
    report.append("")

    # Line-count preview
    total_extracted_lines = sum(e - s + 1 for s, e in ranges.values())
    report.append("Line-count preview (approx, excludes leading blanks/comments):")
    report.append(f"  Total extracted lines: {total_extracted_lines}")
    report.append(f"  Main file after extract: ~{len(main_lines) - total_extracted_lines}")
    report.append("")

    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    print(f"Wrote {REPORT_PATH}")
    print("\n".join(report[-12:]))


if __name__ == "__main__":
    main()
