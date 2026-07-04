#!/usr/bin/env python3
"""Comprehensive audit of the GC refactor.

Checks:
  1. Every GBE_* declaration in gbe_dota_gc_internal.h has a matching
     definition somewhere in the GC TUs (find zombie declarations).
  2. Every shared GBE_* free-function definition has a declaration in the
     header, while member/static helpers are classified as non-actionable.
  3. Doc line numbers in REFACTOR_TODO.md match actual code.
"""
import os
import re
import glob

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
INTERNAL_H = os.path.join(ROOT_DIR, "dll", "gbe_dota_gc_internal.h")
PUBLIC_HEADERS = [
    os.path.join(ROOT_DIR, "dll", "dll", "gbe_dota_reconnect_shared.h"),
]
MAIN_CPP = os.path.join(ROOT_DIR, "dll", "steam_game_coordinator.cpp")
TODO_MD = os.path.join(ROOT_DIR, "REFACTOR_TODO.md")
GC_TUS = sorted(glob.glob(os.path.join(ROOT_DIR, "dll", "gbe_dota_*.cpp"))) + [MAIN_CPP]

POST_LOGIN_DISPATCH_ENTRIES = [
    ("GBE_kDotaJoinChatChannel", "adapt_join_chat_channel", "GBE_HandleDotaJoinChatChannelRequest"),
    ("GBE_kDotaPracticeLobbyCreate", "adapt_practice_lobby_create", "GBE_HandleDotaPracticeLobbyCreateRequest"),
    ("GBE_kDotaLobbyList", "adapt_lobby_list", "GBE_HandleDotaLobbyListRequest"),
    ("GBE_kDotaCustomLobbyListRequest", "adapt_custom_lobby_list", "GBE_HandleDotaCustomLobbyListRequest"),
    ("GBE_kDotaFriendPracticeLobbyListRequest", "adapt_friend_practice_lobby_list", "GBE_HandleDotaFriendPracticeLobbyListRequest"),
    ("GBE_kGCInviteToLobby", "adapt_invite_to_lobby", "GBE_HandleDotaInviteToLobbyRequest"),
    ("GBE_kGCLobbyInviteResponse", "adapt_lobby_invite_response", "GBE_HandleDotaLobbyInviteResponseRequest"),
    ("GBE_kDotaPracticeLobbyJoin", "adapt_practice_lobby_join", "GBE_HandleDotaPracticeLobbyJoinRequest"),
    ("GBE_kDotaPracticeLobbyLeave", "adapt_practice_lobby_leave", "GBE_HandleDotaPracticeLobbyLeaveRequest"),
    ("GBE_kDotaPracticeLobbyLaunch", "adapt_practice_lobby_launch", "GBE_HandleDotaPracticeLobbyLaunchRequest"),
    ("GBE_kDotaPracticeLobbySetDetails", "adapt_practice_lobby_set_details", "GBE_HandleDotaPracticeLobbySetDetailsRequest"),
    ("GBE_kDotaPracticeLobbySetTeamSlot", "adapt_practice_lobby_set_team_slot", "GBE_HandleDotaPracticeLobbySetTeamSlotRequest"),
    ("GBE_kDotaPracticeLobbyKick", "adapt_practice_lobby_kick", "GBE_HandleDotaPracticeLobbyKickRequest"),
    ("GBE_kDotaPracticeLobbyJoinBroadcastChannel", "adapt_practice_lobby_join_broadcast", "GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest"),
    ("GBE_kDotaLobbyUpdateBroadcastChannelInfo", "adapt_lobby_update_broadcast_info", "GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest"),
    ("GBE_kDotaPracticeLobbyCloseBroadcastChannel", "adapt_practice_lobby_close_broadcast", "GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest"),
]


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def extract_header_symbols(header_text):
    """Extract external GBE_* function/variable declarations from the header."""
    symbols = set()
    declaration = ""
    for raw_line in header_text.splitlines():
        line = raw_line.split("//", 1)[0].strip()
        if not line:
            continue
        if line.startswith(("struct ", "class ", "using ", "template ", "#")):
            declaration = ""
            continue

        declaration = (declaration + " " + line).strip()
        if ";" not in declaration:
            continue

        decl = declaration.replace("\t", " ")
        declaration = ""

        function_match = re.search(r"\b(GBE_\w+)\s*\(", decl)
        if function_match:
            symbols.add(function_match.group(1))
            continue

        variable_match = re.match(r"^extern\s+.*\b(GBE_\w+)\b\s*(?:\[[^\]]*\])?\s*;", decl)
        if variable_match:
            symbols.add(variable_match.group(1))
    return symbols


def extract_defined_symbols(tu_paths):
    """Extract GBE_* symbols that are DEFINED (have a body) in the TUs.

    A definition is a line starting with a type and containing GBE_* followed
    by '(' and later a '{' on the same or following lines. We approximate:
    match lines like `bool GBE_Foo(` or `extern const ... GBE_Foo =` that are
    NOT terminated with ';' on the first line (i.e., multi-line def) OR are
    single-line `= ...` assignments.
    """
    defined = {}  # name -> (file, lineno, kind)
    for path in tu_paths:
        lines = read(path).splitlines()
        n = len(lines)
        for i, ln in enumerate(lines):
            # member function definitions: "Type Steam_Game_Coordinator::GBE_Foo("
            m = re.match(r"^[A-Za-z_][\w:&*\s<>,]*\bSteam_Game_Coordinator::(GBE_\w+)\s*\(", ln)
            if m and not ln.rstrip().endswith(";"):
                name = m.group(1)
                for j in range(i, min(i + 40, n)):
                    if "{" in lines[j]:
                        defined[name] = (os.path.basename(path), i + 1, "member_function")
                        break
                    if lines[j].rstrip().endswith(";"):
                        break
                continue

            # function definition: "<type> GBE_Foo(" not ending with ';'
            m = re.match(r"^[A-Za-z_][\w:&*\s<>,]*\b(GBE_\w+)\s*\(", ln)
            if m and not ln.rstrip().endswith(";"):
                name = m.group(1)
                # Confirm there's a '{' within next few lines (definition body).
                for j in range(i, min(i + 40, n)):
                    if "{" in lines[j]:
                        kind = "static_function" if ln.lstrip().startswith("static ") else "free_function"
                        defined[name] = (os.path.basename(path), i + 1, kind)
                        break
                    if lines[j].rstrip().endswith(";"):
                        break  # declaration, not definition
                continue
            # extern const var definition: "extern const ... GBE_Foo ="
            m = re.match(r"^extern\s+.*\b(GBE_\w+)\s*=", ln)
            if m:
                defined[m.group(1)] = (os.path.basename(path), i + 1, "variable")
                continue
            m = re.match(r"^(?!static\b)[A-Za-z_][\w:&*\s<>,]*\b(GBE_\w+)\s*(?:\[[^\]]*\])?\s*(?:\{|=|;)", ln)
            if m:
                defined[m.group(1)] = (os.path.basename(path), i + 1, "variable")
    return defined


def audit_post_login_dispatch(main_text):
    start = main_text.find("bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest")
    if start < 0:
        return ["GBE_DispatchDotaPostLoginRequest definition not found"]
    end = main_text.find("bool Steam_Game_Coordinator::gc_enabled", start)
    dispatch_text = main_text[start:end if end >= 0 else len(main_text)]

    issues = []
    for emsg, adapter, handler in POST_LOGIN_DISPATCH_ENTRIES:
        table_pattern = re.compile(r"\{\s*" + re.escape(emsg) + r"\s*,\s*" + re.escape(adapter) + r"\s*\}")
        if not table_pattern.search(dispatch_text):
            issues.append(f"{emsg}: missing table entry for {adapter}")

        adapter_pattern = re.compile(
            r"auto\s+" + re.escape(adapter) + r"\s*=.*?return\s+self->" + re.escape(handler) + r"\s*\(",
            re.DOTALL,
        )
        if not adapter_pattern.search(dispatch_text):
            issues.append(f"{adapter}: missing adapter call to {handler}")

    found_entries = re.findall(r"\{\s*(GBE_k[A-Za-z0-9_]+)\s*,\s*(adapt_[A-Za-z0-9_]+)\s*\}", dispatch_text)
    expected_pairs = {(emsg, adapter) for emsg, adapter, _ in POST_LOGIN_DISPATCH_ENTRIES}
    found_pairs = set(found_entries)
    for emsg, adapter in sorted(found_pairs - expected_pairs):
        issues.append(f"{emsg}: unexpected dispatch table adapter {adapter}")

    return issues


def main():
    header_text = read(INTERNAL_H)
    real_decls = extract_header_symbols(header_text)
    all_declared_symbols = set(real_decls)
    for header in PUBLIC_HEADERS:
        if os.path.exists(header):
            all_declared_symbols.update(extract_header_symbols(read(header)))

    defined = extract_defined_symbols(GC_TUS)

    print("=" * 70)
    print("AUDIT 1: Header declarations WITHOUT any definition (zombie decls)")
    print("=" * 70)
    print("  Action: remove stale declarations or restore the missing definition.")
    print("  False-positive class: declarations for non-GBE types are ignored.")
    zombies = sorted(real_decls - set(defined.keys()))
    if not zombies:
        print("  (none) - all header declarations have a definition")
    else:
        for z in zombies:
            print(f"  GBE_{z[4:]}: declared in header but NO definition found in any TU")
    print()

    print("=" * 70)
    print("AUDIT 2: Definitions WITHOUT header declaration (under-exposed)")
    print("=" * 70)
    print("  Action: declare shared helpers, or make private helpers file-local.")
    print("  False-positive class: member functions and static helpers are non-actionable.")
    # Only check free functions in shared orchestration TUs. Member functions are
    # visible via class headers, and static helpers intentionally stay file-local.
    underexposed = []
    for name, (f, ln, kind) in sorted(defined.items(), key=lambda x: (x[1][0], x[1][1])):
        if name not in all_declared_symbols:
            # Check if it's a member function (visible via class header, OK)
            # or a static (internal, OK). We approximate: if defined in main/payload
            # TU as a free function and not in header, flag it.
            if kind == "free_function" and f in ("steam_game_coordinator.cpp", "gbe_dota_gc_payload_helpers.cpp"):
                underexposed.append((name, f, ln))
    if not underexposed:
        print("  (none) - all shared free-function/extern-var definitions are header-declared")
    else:
        for name, f, ln in underexposed:
            print(f"  GBE_{name[4:]}: defined at {f}:{ln} but NOT in header")
    print()

    print("=" * 70)
    print("AUDIT 3: Doc line-number accuracy (REFACTOR_TODO.md vs actual)")
    print("=" * 70)
    print("  Action: update fragile line references or replace them with function names.")
    print("  False-positive class: function-name references without L<num> are ignored.")
    main_text = read(MAIN_CPP)
    main_lines = main_text.splitlines()
    todo_text = read(TODO_MD)
    # Extract doc-claimed member function positions: "L<digits>  <name>"
    doc_fns = {}
    for m in re.finditer(r"^L(\d+)\s+(\S+)", todo_text, re.MULTILINE):
        doc_fns[m.group(2)] = int(m.group(1))
    mismatches = []
    for fn, claimed_line in doc_fns.items():
        actual = None
        for i, ln in enumerate(main_lines):
            if re.search(r"\bSteam_Game_Coordinator::" + re.escape(fn) + r"\s*\(", ln):
                actual = i + 1
                break
        if actual is None:
            mismatches.append((fn, claimed_line, "NOT FOUND in main file"))
        elif abs(actual - claimed_line) > 2:
            mismatches.append((fn, claimed_line, f"actual L{actual}"))
    if not mismatches:
        print(f"  All {len(doc_fns)} documented line numbers match (within +-2 lines)")
    else:
        for fn, claimed, msg in mismatches:
            print(f"  {fn}: doc says L{claimed}, {msg}")
    print()

    print("=" * 70)
    print("AUDIT 4: Post-login dispatch table mapping")
    print("=" * 70)
    print("  Action: keep the dispatch table aligned with the original post-login switch mapping.")
    dispatch_issues = audit_post_login_dispatch(main_text)
    if not dispatch_issues:
        print(f"  All {len(POST_LOGIN_DISPATCH_ENTRIES)} dispatch entries map to the expected handlers")
    else:
        for issue in dispatch_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"  Header extern/function declarations: {len(real_decls)}")
    print(f"  Known public header declarations:     {len(all_declared_symbols)}")
    print(f"  Definitions found across all TUs:    {len(defined)}")
    print(f"  Zombie declarations (no def):        {len(zombies)}")
    print(f"  Under-exposed definitions:           {len(underexposed)}")
    print(f"  Doc line-number mismatches:          {len(mismatches)}")
    print(f"  Dispatch table mismatches:           {len(dispatch_issues)}")


if __name__ == "__main__":
    main()
