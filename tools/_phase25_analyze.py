#!/usr/bin/env python3
"""Phase 2.5 Step 1: Coupling analysis for extracting lobby-state build/snapshot helpers.

Targets 8 Steam_Game_Coordinator:: member function NAMES in steam_game_coordinator.cpp
(two of which have 2 overloads each, so 12 definitions total) that are candidates to
move to a new TU (gbe_dota_lobby_snapshot_coordinator.cpp):

  GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay  (x2 overloads)
  GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload          (x2 overloads)
  GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate
  GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots
  GBE_GetDotaGenericLobbySnapshots
  GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot
  GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed
  GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate

Two file-scope static Impl helpers are expected to fall into List X automatically
(they are referenced only from the target overloads):
  GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl
  GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl

For each file-scope `static` symbol in the main file, this script determines
whether it is referenced from within the target functions' bodies, and
classifies it as:

  List X  — referenced ONLY from target functions  -> can move with them as `static`
  List Y  — referenced from target AND other code  -> must be externalized
  List Z  — not referenced from target functions   -> stays in main file unchanged

It also lists symbols already externalized (per Phase 2.2 / 2.3a / 2.4a) that are
referenced from the target functions, to verify the prep work is sufficient.

Output: tools/_phase25_coupling_report.txt
"""
import os
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
REPORT_PATH = "/workspace/tools/_phase25_coupling_report.txt"

# 8 member function names. Overloads are handled by find_all_member_function_starts.
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

# Symbols already externalized in earlier phases (Phase 2.3a + 2.4a).
# These have external linkage and live in steam_game_coordinator.cpp with
# extern declarations in gbe_dota_gc_internal.h. We track their usage from
# the target functions to confirm no further externalization is missed.
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
}


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
    Returns list of dicts: {name, start_idx, end_idx, kind, signature}."""
    statics = []
    for i, line in enumerate(lines):
        if not line.startswith("static "):
            continue
        m = re.search(r'\b(GBE_\w+|ser_\w+|deser_\w+)\s*[\(\{=;]', line)
        if not m:
            m = re.search(r'\bstatic\s+(?:const\s+|constexpr\s+)*[\w:<>,\s\*\(\)]*?\b(\w+)\s*[\(\{=;]', line)
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


def find_variable_end(lines, start):
    """Find the next line whose stripped content ends with ';'."""
    for i in range(start, len(lines)):
        if lines[i].rstrip().endswith(";"):
            return i
    raise RuntimeError(f"Could not find terminating ';' for variable at line {start+1}")


def find_references(lines, symbol_name):
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
    with open(CPP_PATH, "r") as f:
        raw = f.read()
    lines = raw.split("\n")

    print(f"Loaded {CPP_PATH}: {len(lines)} lines")

    # --- 1. Locate all target function definitions (including overloads) ---
    target_ranges = []  # list of (start_idx, end_idx, func_name)
    print("\n=== Target function boundaries (incl. overloads) ===")
    for fname in TARGET_FUNCTIONS:
        for s in find_all_member_function_starts(lines, fname):
            e = find_function_end(lines, s)
            target_ranges.append((s, e, fname))
    # Sort by start line for readable output
    target_ranges.sort(key=lambda x: x[0])
    for s, e, fname in target_ranges:
        print(f"  {fname:60s}  lines {s+1:5d}-{e+1:5d}  ({e-s+1} lines)")

    target_sorted = sorted([(s, e) for s, e, _ in target_ranges])

    # --- 2. Collect all file-scope static symbols ---
    statics = collect_static_symbols(lines)
    print(f"\n=== Found {len(statics)} file-scope `static` symbols ===")

    by_name = {}
    for s in statics:
        if s["name"] not in by_name:
            by_name[s["name"]] = s
        else:
            existing = by_name[s["name"]]
            if (s["end"] - s["start"]) > (existing["end"] - existing["start"]):
                by_name[s["name"]] = s
    unique_statics = sorted(by_name.values(), key=lambda x: x["start"])

    # --- 3. For each static symbol, find references and classify ---
    list_x = []
    list_y = []
    list_z = []
    list_unused = []

    for s in unique_statics:
        name = s["name"]
        def_start = s["start"]
        def_end = s["end"]
        all_refs = []
        for ridx in find_references(lines, name):
            if def_start <= ridx <= def_end:
                continue
            all_refs.append(ridx)
        target_refs = [r for r in all_refs if line_in_ranges(r, target_sorted)]
        other_refs = [r for r in all_refs if r not in set(target_refs)]

        if not all_refs:
            list_unused.append(s)
        elif target_refs and not other_refs:
            list_x.append((s, target_refs))
        elif target_refs and other_refs:
            list_y.append((s, target_refs, other_refs))
        else:
            list_z.append(s)

    # --- 4. Find references from target functions to already-externalized symbols ---
    external_refs = []
    for sym in ALREADY_EXTERNAL:
        refs = []
        for ridx in find_references(lines, sym):
            if line_in_ranges(ridx, target_sorted):
                refs.append(ridx)
        if refs:
            external_refs.append((sym, refs))

    # --- 5. Find references from target functions to other Steam_Game_Coordinator::
    #       member functions (method calls; shared via class declaration, no action). ---
    member_refs = {}
    member_pat = re.compile(r'^[a-zA-Z_][\w:&*<>,\s\(\)]*?\bSteam_Game_Coordinator::(\w+)\s*\(')
    member_names = set()
    for line in lines:
        m = member_pat.match(line)
        if m:
            member_names.add(m.group(1))
    member_names.update(TARGET_FUNCTIONS)
    # Also include member functions now living in other split TUs (handlers, lobby-state,
    # lobby-flow) — they're all Steam_Game_Coordinator:: members accessible via the header.
    # We can't enumerate them from the main file alone, but calls to them appear as
    # `FUNC_NAME(` or `this->FUNC_NAME(` or `target->FUNC_NAME(` inside target bodies.
    call_pat = re.compile(r'\b(?:this->|target->)?(\w+)\s*\(')
    for s, e, fname in target_ranges:
        for ridx in range(s, e+1):
            for m in call_pat.finditer(lines[ridx]):
                callee = m.group(1)
                if callee in member_names and callee != fname:
                    member_refs.setdefault(callee, set()).add(ridx)

    # --- 6. Write report ---
    report_lines = []
    def out(text=""):
        report_lines.append(text)
        print(text)

    out("=" * 80)
    out("PHASE 2.5: LOBBY-STATE BUILD/SNAPSHOT HELPER COUPLING ANALYSIS REPORT")
    out(f"File: {CPP_PATH} ({len(lines)} lines)")
    out("Purpose: Support refactoring — extracting 8 member function names")
    out("         (12 definitions incl. overloads) to a new TU")
    out("         (gbe_dota_lobby_snapshot_coordinator.cpp)")
    out("=" * 80)
    out("")
    out("METHODOLOGY")
    out("-----------")
    out("- 8 target member function NAMES were located by matching")
    out("  `^.*Steam_Game_Coordinator::FUNC_NAME(` at column 0.")
    out("- Overloads are ALL included (find_all_member_function_starts).")
    out("- Function end was determined by brace-depth tracking (strings/comments")
    out("  stripped per-line).")
    out("- All file-scope `static` symbols (column-0 `static` keyword) were")
    out("  collected, deduplicated by name (forward decl + definition merged).")
    out("- Each static symbol's references were found with word-boundary regex")
    out("  across the entire main file, excluding the definition's own span.")
    out("- References were classified as 'inside target' or 'elsewhere'.")
    out("")
    out("TARGET FUNCTIONS (incl. overloads)")
    out("----------------------------------")
    for s, e, fname in target_ranges:
        out(f"  {fname:60s}  lines {s+1:5d}-{e+1:5d}  ({e-s+1} lines)")
    total_target_lines = sum(e - s + 1 for s, e, _ in target_ranges)
    out(f"  {'TOTAL':60s}  {total_target_lines} lines across {len(target_ranges)} definitions")
    out("  ({0} unique names, {1} with 2 overloads)".format(
        len(TARGET_FUNCTIONS),
        len([t for t in TARGET_FUNCTIONS if len(find_all_member_function_starts(lines, t)) > 1])))
    out("")

    out("=" * 80)
    out(f"LIST X — TARGET-ONLY STATICS (can move to new TU as `static`)")
    out(f"Count: {len(list_x)}")
    out("=" * 80)
    out("These static symbols are referenced ONLY from within the target")
    out("functions. They can be moved verbatim to the new TU and remain")
    out("file-scope `static` there. (The two *Impl helpers are expected here.)")
    out("")
    out(" #  Def Line  Kind       Symbol Name                                  Target Ref Lines")
    out("--  --------  ---------  -------------------------------------------  ----------------")
    for i, (s, trefs) in enumerate(list_x, 1):
        ref_str = ", ".join(str(r+1) for r in trefs[:8])
        if len(trefs) > 8:
            ref_str += f", +{len(trefs)-8} more"
        out(f"{i:>2}  {s['start']+1:>8}  {s['kind']:<9}  {s['name']:<43}  {ref_str}")
    out("")

    out("=" * 80)
    out(f"LIST Y — SHARED STATICS (must be externalized before move)")
    out(f"Count: {len(list_y)}")
    out("=" * 80)
    out("These static symbols are referenced from BOTH the target functions AND")
    out("other code in the main file. They MUST have `static` removed and be")
    out("declared `extern` in gbe_dota_gc_internal.h before the move.")
    out("Definitions stay in steam_game_coordinator.cpp.")
    out("")
    if list_y:
        out(" #  Def Line  Kind       Symbol Name                                  Target Refs  Other Refs")
        out("--  --------  ---------  -------------------------------------------  -----------  ----------")
        for i, (s, trefs, orefs) in enumerate(list_y, 1):
            out(f"{i:>2}  {s['start']+1:>8}  {s['kind']:<9}  {s['name']:<43}  "
                f"{len(trefs):>11}  {len(orefs):>10}")
            sample_t = ", ".join(str(r+1) for r in trefs[:5])
            sample_o = ", ".join(str(r+1) for r in orefs[:5])
            out(f"                                              target refs: {sample_t}")
            out(f"                                              other  refs: {sample_o}")
    else:
        out("  (none)")
    out("")

    out("=" * 80)
    out(f"LIST Z — NON-TARGET STATICS (stay in main file unchanged)")
    out(f"Count: {len(list_z)}")
    out("=" * 80)
    out("These static symbols are NOT referenced from any target function.")
    out("They stay in steam_game_coordinator.cpp with internal linkage unchanged.")
    out("")
    out(" #  Def Line  Kind       Symbol Name")
    out("--  --------  ---------  -------------------------------------------")
    for i, s in enumerate(list_z, 1):
        out(f"{i:>2}  {s['start']+1:>8}  {s['kind']:<9}  {s['name']}")
    out("")

    out("=" * 80)
    out(f"UNUSED — DEFINED BUT NEVER REFERENCED (dead code)")
    out(f"Count: {len(list_unused)}")
    out("=" * 80)
    if list_unused:
        out(" #  Def Line  Kind       Symbol Name")
        out("--  --------  ---------  -------------------------------------------")
        for i, s in enumerate(list_unused, 1):
            out(f"{i:>2}  {s['start']+1:>8}  {s['kind']:<9}  {s['name']}")
    else:
        out("  (none)")
    out("")

    out("=" * 80)
    out("ALREADY-EXTERNALIZED SYMBOLS REFERENCED FROM TARGET FUNCTIONS")
    out(f"Count: {len(external_refs)}")
    out("=" * 80)
    out("These symbols were promoted to external linkage in Phase 2.2 / 2.3a / 2.4a.")
    out("Their presence here confirms the prep work covers the target functions'")
    out("needs. No further action required for these.")
    out("")
    if external_refs:
        out(" #  Symbol Name                                  Ref Lines (in target bodies)")
        out("--  -------------------------------------------  --------------------------")
        for i, (sym, refs) in enumerate(external_refs, 1):
            ref_str = ", ".join(str(r+1) for r in refs[:10])
            if len(refs) > 10:
                ref_str += f", +{len(refs)-10} more"
            out(f"{i:>2}  {sym:<43}  {ref_str}")
    else:
        out("  (none)")
    out("")

    out("=" * 80)
    out("STEAM_GAME_COORDINATOR:: MEMBER CALLS FROM TARGET FUNCTIONS")
    out("=" * 80)
    out("These are calls to other member functions of the same class. They are")
    out("already shared via the class declaration in dll/steam_game_coordinator.h")
    out("and require NO externalization. Listed for completeness.")
    out("")
    if member_refs:
        out(" #  Member Function                              Call Sites")
        out("--  -------------------------------------------  ----------")
        for i, (mname, sites) in enumerate(sorted(member_refs.items()), 1):
            sample = ", ".join(str(s+1) for s in sorted(sites)[:8])
            if len(sites) > 8:
                sample += f", +{len(sites)-8} more"
            out(f"{i:>2}  {mname:<43}  {sample}")
    else:
        out("  (none)")
    out("")

    out("=" * 80)
    out("SYMBOL COUNT RECONCILIATION")
    out("=" * 80)
    out(f"  Total unique file-scope `static` symbols:  {len(unique_statics)}")
    out(f"  List X (target-only, move as static):      {len(list_x)}")
    out(f"  List Y (shared, must externalize):         {len(list_y)}")
    out(f"  List Z (non-target, stays):                {len(list_z)}")
    out(f"  Unused (dead code):                        {len(list_unused)}")
    out(f"  Already-external referenced from target:   {len(external_refs)}")
    out("")

    out("=" * 80)
    out("REFACTORING IMPLICATIONS")
    out("=" * 80)
    if list_y:
        out(f"1. PHASE 2.5-step2 REQUIRED: Externalize {len(list_y)} List Y static")
        out("   symbols. Write tools/_phase25_externalize.py to:")
        out("   - Remove `static ` prefix from List Y function definitions")
        out("   - Replace `static const` -> `extern const` for List Y const arrays")
        out("   - Demote `static constexpr` -> `extern const` for List Y constexpr data")
        out("   - Append extern declarations to gbe_dota_gc_internal.h")
        out("   Definitions stay in steam_game_coordinator.cpp.")
    else:
        out("1. PHASE 2.5-step2 SKIPPABLE: No List Y symbols — earlier phases'")
        out("   externalization work already covers all cross-TU sharing needs.")
    out("")
    if list_x:
        out(f"2. PHASE 2.5-step3: Move {len(list_x)} List X static symbols + 12 target")
        out("   definitions (8 names, 2 with overloads) to")
        out("   gbe_dota_lobby_snapshot_coordinator.cpp. List X symbols keep their")
        out("   `static` keyword (internal to the new TU).")
    else:
        out("2. PHASE 2.5-step3: No List X static symbols to move — only the 12")
        out("   target definitions need to be moved.")
    out("")
    out("3. PREMAKE: No change needed — premake5.lua's common_files uses")
    out("   `\"dll/**\"` glob to auto-collect all sources under dll/.")
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
