#!/usr/bin/env python3
"""Phase 2.9 Step 1: Coupling analysis for extracting file-scope static helpers.

Unlike prior phases (which moved Steam_Game_Coordinator:: member functions),
Phase 2.9 targets the file-scope `static` helper FUNCTIONS/VARIABLES that
remain in steam_game_coordinator.cpp. These are pure free functions (not
members) used by the GC code.

Goal: move as many static helpers as practical to a new TU
(gbe_dota_gc_payload_helpers.cpp), externalizing those that are still
called from code that stays in the main file.

Classification (different from prior phases):

  List X  — referenced ONLY from other static helpers (targets)  -> move as `static`
  List Y  — referenced from main-file member functions (stays)   -> externalize (move def, add extern)
  List Z  — referenced from OTHER TUs (already externalized?)    -> verify; usually stays or already external
  Unused  — no refs at all                                        -> dead code

For each static symbol, references are searched across:
  - steam_game_coordinator.cpp (main file: member funcs + other statics)
  - all other split TUs (gbe_dota_*.cpp) — to catch cross-TU refs that would
    require the symbol to already be externalized

Output: tools/_phase29_coupling_report.txt
"""
import os
import re
import sys
import glob

MAIN_CPP = "/workspace/dll/steam_game_coordinator.cpp"
REPORT_PATH = "/workspace/tools/_phase29_coupling_report.txt"

# All split TUs that share the GC class (member functions live in these).
SPLIT_TUS = sorted(glob.glob("/workspace/dll/gbe_dota_*.cpp"))

# Symbols already externalized in earlier phases. These have external linkage,
# live in steam_game_coordinator.cpp with extern declarations in
# gbe_dota_gc_internal.h. They are NOT file-scope `static` anymore.
ALREADY_EXTERNAL = {
    # Phase 2.3a — mutable state
    "GBE_pending_dota_normal_signout_finalize_after_25",
    "GBE_pending_dota_normal_signout_finalize_lobby_id",
    "GBE_vpk_loot_data",
    # Phase 2.3a — const data tables
    "GBE_kOldDotaAccountIdVarint",
    "GBE_kOldDotaSteamIdVarint",
    "GBE_kOldDotaLobbyIdVarint",
    "GBE_kOldDotaSteamIdFixed64",
    "GBE_kOldDotaPersonaSteamIdFixed64",
    "GBE_kOldDotaAccountIdFixed32",
    "GBE_kOldDotaPracticeLobbyMatchIdVarint",
    "GBE_kOldDotaPracticeLobbyServerIdFixed64",
    "GBE_kOldDotaPracticeLobbyLobbyIdText",
    "GBE_kOldDotaPracticeLobbyLobbyIdTextAlt",
    "GBE_kSteamTicketAuthComplete",
    "GBE_kDotaAbandonPersonaStateInitHex",
    # Phase 2.3a — functions
    "GBE_PushDotaPlayerEquippedItemsCacheToGC",
    "GBE_RewriteAccountIdVarintInDirectProtoBody",
    "GBE_TryPatchDotaAccountIdVarint",
    "GBE_TryPatchDotaAccountIdFixed32",
    "GBE_DotaCustomGameDisplayName",
    "GBE_PatchDotaTemplateIdentifiers",
    "GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage",
    "GBE_PrepareDotaPersonaStatePeripheralMessage",
    "GBE_LogDotaResponsePacket",
    "GBE_PrepareDotaDirectReplayMessage",
    # Phase 2.2 / 2.3a — shared state & helpers from internal header
    "GBE_shared_dota_lobby_state",
    "GBE_recent_dota_reconnect_context_valid",
    "GBE_recent_dota_reconnect_context",
    "GBE_GetDotaReconnectContext",
    "GBE_IsDotaArcadeLobbyActive",
    "GBE_TryRecoverDotaReconnectContextFromGenericLobbies",
    "GBE_GC_DebugLog",
    "GBE_DescribeDotaLaunchPhase",
    "GBE_LogDotaSOCacheSubscribedSummary",
    "GBE_AdaptDotaJoinChatChannelResponsePayload",
    # Phase 2.4a — server hello context
    "GBE_BuildDirectDotaServerWelcome",
    "GBE_last_dota_server_hello_context",
    "GBE_DotaServerHelloContext",
    # Phase 2.5 — lobby-snapshot/build shared symbols
    "GBE_kDotaOfficial032PracticeLobby26Hex",
    "GBE_ReplayDotaPracticeLobbyOfficial26Payload",
    "GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate",
    "GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload",
    "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl",
    "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl",
    # Phase 2.6 — List X statics moved to lobby_launch TU (now static there, not external)
    "GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex",
    "GBE_GenerateDotaPostGameChatChannelId",
    # Phase 2.7 — moved to inventory TU (static there)
    "ser_varstring",
    # Phase 2.8 — moved to welcome TU (static there)
    "GBE_kDotaCacheSubscribedTemplate",
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
    Handles functions, variables, arrays, and overloads."""
    statics = []
    for i, line in enumerate(lines):
        if not line.startswith("static "):
            continue
        # Extract symbol name: handle GBE_*, ser_*, deser_* and generic names
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
            "signature": line.rstrip(),
        })
    return statics


def find_member_function_ranges(lines):
    """Find all (start, end, name) ranges of Steam_Game_Coordinator:: member
    function definitions in the main file."""
    pat = re.compile(r'^[a-zA-Z_][\w:&*<>,\s\(\)]*?\bSteam_Game_Coordinator::(\w+)\s*\(')
    ranges = []
    for i, line in enumerate(lines):
        m = pat.match(line)
        if m:
            name = m.group(1)
            try:
                e = find_function_end(lines, i)
                ranges.append((i, e, name))
            except RuntimeError:
                pass
    return ranges


def find_references_in_lines(lines, symbol_name):
    """Find all line indices where `symbol_name` appears as a word-boundary token."""
    pat = re.compile(r'\b' + re.escape(symbol_name) + r'\b')
    refs = []
    for i, line in enumerate(lines):
        if pat.search(line):
            refs.append(i)
    return refs


def line_in_ranges(idx, ranges):
    """Check if line index falls within any (start, end) range (inclusive)."""
    for s, e in ranges:
        if s <= idx <= e:
            return True
    return False


def main():
    with open(MAIN_CPP, "r") as f:
        raw = f.read()
    main_lines = raw.split("\n")

    print(f"Loaded {MAIN_CPP}: {len(main_lines)} lines")

    # 1. Collect all file-scope static symbols in main file
    statics = collect_static_symbols(main_lines)
    print(f"\n=== Found {len(statics)} file-scope `static` symbol definitions ===")

    # Deduplicate by (name, start) keeping overloads separate
    # For classification, treat each definition; refs to a name cover all overloads.
    unique_names = sorted(set(s["name"] for s in statics))
    # Build name -> list of definitions
    by_name = {}
    for s in statics:
        by_name.setdefault(s["name"], []).append(s)

    # All static definition ranges (any overload)
    all_static_ranges = [(s["start"], s["end"]) for s in statics]

    # 2. Find member function ranges in main file (these STAY)
    member_ranges = find_member_function_ranges(main_lines)
    print(f"=== Found {len(member_ranges)} Steam_Game_Coordinator:: member function definitions (stay) ===")

    # 3. Load other split TUs
    split_tu_lines = {}
    for tu in SPLIT_TUS:
        with open(tu, "r") as f:
            split_tu_lines[tu] = f.read().split("\n")
        print(f"  Loaded split TU: {os.path.basename(tu)} ({len(split_tu_lines[tu])} lines)")

    # 4. For each static symbol (by name), find references:
    #    - in main file static defs (other statics)
    #    - in main file member funcs (stays)
    #    - in main file other (non-static, non-member top-level — rare)
    #    - in other split TUs (cross-TU refs)
    list_x = []   # only referenced from other statics (targets)
    list_y = []   # referenced from main member funcs (stays) -> must externalize
    list_z = []   # referenced from other TUs (cross-TU) -> verify
    list_unused = []

    for name in unique_names:
        # Refs in main file, excluding the definition(s) themselves
        own_defs = by_name[name]
        own_ranges = [(s["start"], s["end"]) for s in own_defs]

        main_refs = []
        for ridx in find_references_in_lines(main_lines, name):
            if line_in_ranges(ridx, own_ranges):
                continue
            main_refs.append(ridx)

        main_static_refs = [r for r in main_refs if line_in_ranges(r, all_static_ranges)]
        main_member_refs = [r for r in main_refs if line_in_ranges(r, [(s, e) for s, e, _ in member_ranges])]
        main_other_refs = [r for r in main_refs if r not in set(main_static_refs) and r not in set(main_member_refs)]

        # Cross-TU refs
        cross_tu_refs = {}
        for tu, tu_lines in split_tu_lines.items():
            refs = find_references_in_lines(tu_lines, name)
            if refs:
                cross_tu_refs[tu] = refs

        if not main_refs and not cross_tu_refs:
            list_unused.append((name, own_defs))
        elif main_member_refs and not main_static_refs and not cross_tu_refs:
            # Only called from member funcs (stays) -> externalize to move, OR just leave
            list_y.append((name, own_defs, main_member_refs, main_other_refs, cross_tu_refs))
        elif main_static_refs and not main_member_refs and not cross_tu_refs:
            # Only called from other statics -> can move as static (if callers also move)
            list_x.append((name, own_defs, main_static_refs))
        elif cross_tu_refs and not main_member_refs and not main_static_refs:
            # Only cross-TU refs -> should already be external; verify
            list_z.append((name, own_defs, main_static_refs, main_member_refs, cross_tu_refs))
        else:
            # Mixed: referenced from multiple sources -> externalize
            list_y.append((name, own_defs, main_member_refs, main_static_refs + main_other_refs, cross_tu_refs))

    # 5. Write report
    report_lines = []
    def out(text=""):
        report_lines.append(text)
        print(text)

    out("=" * 80)
    out("PHASE 2.9: STATIC HELPERS COUPLING ANALYSIS REPORT")
    out(f"Main file: {MAIN_CPP} ({len(main_lines)} lines)")
    out(f"Split TUs scanned: {len(SPLIT_TUS)}")
    out("Purpose: Support refactoring — extracting file-scope `static` helpers")
    out("         to a new TU (gbe_dota_gc_payload_helpers.cpp).")
    out("=" * 80)
    out("")
    out("METHODOLOGY")
    out("-----------")
    out("- All file-scope `static` symbols in main file collected (functions + vars).")
    out("- Member function (Steam_Game_Coordinator::) ranges identified as 'stays'.")
    out("- For each static symbol, references classified by source:")
    out("    * main static defs (other targets)")
    out("    * main member funcs (stays)  -> externalize required to move")
    out("    * other split TUs            -> cross-TU (verify externalization)")
    out("- List X: only static refs (move as `static`).")
    out("- List Y: member-func refs (externalize to move, OR leave in main).")
    out("- List Z: cross-TU refs (verify already external).")
    out("")

    out("=" * 80)
    out("ALL FILE-SCOPE `static` SYMBOLS IN MAIN FILE")
    out(f"Count: {len(statics)} definitions ({len(unique_names)} unique names)")
    out("=" * 80)
    out(" #  Def Line  Kind       Symbol Name")
    out("--  --------  ---------  -------------------------------------------")
    for i, s in enumerate(statics, 1):
        out(f"{i:>2}  {s['start']+1:>8}  {s['kind']:<9}  {s['name']}")
    out("")

    out("=" * 80)
    out(f"LIST X — STATIC-ONLY REFS (can move to new TU as `static`)")
    out(f"Count: {len(list_x)}")
    out("=" * 80)
    out("Referenced ONLY from other static helpers. Can move verbatim as `static`.")
    out("CAUTION: verify transitive — callers must also move (else callers lose access).")
    out("")
    if list_x:
        out(" #  Def Line  Kind       Symbol Name                                  Static Ref Lines")
        out("--  --------  ---------  -------------------------------------------  ----------------")
        for i, (name, defs, srefs) in enumerate(list_x, 1):
            ref_str = ", ".join(str(r+1) for r in srefs[:8])
            if len(srefs) > 8:
                ref_str += f", +{len(srefs)-8} more"
            out(f"{i:>2}  {defs[0]['start']+1:>8}  {defs[0]['kind']:<9}  {name:<43}  {ref_str}")
    else:
        out("  (none)")
    out("")

    out("=" * 80)
    out(f"LIST Y — MEMBER-FUNC REFS (must externalize to move, OR leave in main)")
    out(f"Count: {len(list_y)}")
    out("=" * 80)
    out("Referenced from main-file member functions (which stay). To move these,")
    out("`static` must be removed and `extern` declared in gbe_dota_gc_internal.h.")
    out("Alternatively, leave in main file if the helper is small or tightly coupled.")
    out("")
    if list_y:
        out(" #  Def Line  Kind       Symbol Name                                  Member Refs  Static Refs  CrossTU")
        out("--  --------  ---------  -------------------------------------------  -----------  -----------  -------")
        for i, item in enumerate(list_y, 1):
            name, defs, mrefs, srefs, ctu = item
            ctu_count = sum(len(v) for v in ctu.values())
            out(f"{i:>2}  {defs[0]['start']+1:>8}  {defs[0]['kind']:<9}  {name:<43}  "
                f"{len(mrefs):>11}  {len(srefs):>11}  {ctu_count:>7}")
    else:
        out("  (none)")
    out("")

    out("=" * 80)
    out(f"LIST Z — CROSS-TU REFS (verify externalization)")
    out(f"Count: {len(list_z)}")
    out("=" * 80)
    out("Referenced from other split TUs. Since these are `static` (internal linkage),")
    out("cross-TU refs would be a COMPILE ERROR unless the symbol is also defined or")
    out("externalized. Usually means the name collides or is already external.")
    out("")
    if list_z:
        out(" #  Def Line  Kind       Symbol Name                                  Cross-TU Files")
        out("--  --------  ---------  -------------------------------------------  ----------------")
        for i, (name, defs, srefs, mrefs, ctu) in enumerate(list_z, 1):
            files = ", ".join(os.path.basename(t) for t in ctu)
            out(f"{i:>2}  {defs[0]['start']+1:>8}  {defs[0]['kind']:<9}  {name:<43}  {files}")
    else:
        out("  (none)")
    out("")

    out("=" * 80)
    out(f"UNUSED — DEFINED BUT NEVER REFERENCED (dead code)")
    out(f"Count: {len(list_unused)}")
    out("=" * 80)
    if list_unused:
        out(" #  Def Line  Kind       Symbol Name")
        out("--  --------  ---------  -------------------------------------------")
        for i, (name, defs) in enumerate(list_unused, 1):
            out(f"{i:>2}  {defs[0]['start']+1:>8}  {defs[0]['kind']:<9}  {name}")
    else:
        out("  (none)")
    out("")

    # 6. Refactoring implications & size estimate
    out("=" * 80)
    out("REFRACTORING IMPLICATIONS & SIZE ESTIMATE")
    out("=" * 80)
    x_lines = sum(d['end'] - d['start'] + 1 for name, defs, _ in list_x for d in defs)
    y_lines = sum(d['end'] - d['start'] + 1 for item in list_y for d in item[1])
    out(f"  List X symbols total: {x_lines} lines (move as static, no extern needed)")
    out(f"  List Y symbols total: {y_lines} lines (move + extern declaration in header)")
    out(f"  Potential main-file reduction if all X+Y moved: {x_lines + y_lines} lines")
    out("")
    out("NOTE: List Y symbols need extern declarations in gbe_dota_gc_internal.h.")
    out("      List X symbols can move as `static` only if their callers also move.")
    out("      If a List X symbol's caller is List Y, the List X symbol effectively")
    out("      becomes part of the Y cluster (move + extern, or leave).")
    out("")

    out("=" * 80)
    out("END OF REPORT")
    out("=" * 80)

    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w") as f:
        f.write("\n".join(report_lines) + "\n")
    print(f"\nReport written to {REPORT_PATH}")


if __name__ == "__main__":
    main()
