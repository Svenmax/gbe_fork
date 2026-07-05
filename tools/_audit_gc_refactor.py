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
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
INTERNAL_H = os.path.join(ROOT_DIR, "dll", "gbe_dota_gc_internal.h")
PUBLIC_HEADERS = [
    os.path.join(ROOT_DIR, "dll", "gbe_dota_payload_item_helpers.h"),
    os.path.join(ROOT_DIR, "dll", "gbe_dota_payload_lobby_helpers.h"),
    os.path.join(ROOT_DIR, "dll", "gbe_dota_payload_wire_helpers.h"),
    os.path.join(ROOT_DIR, "dll", "dll", "gbe_dota_reconnect_shared.h"),
]
MAIN_CPP = os.path.join(ROOT_DIR, "dll", "steam_game_coordinator.cpp")
TODO_MD = os.path.join(ROOT_DIR, "REFACTOR_TODO.md")
RUN_GC_OFFLINE_TESTS_SH = os.path.join(ROOT_DIR, "tools", "run_gc_offline_tests.sh")
PREMAKE5_LUA = os.path.join(ROOT_DIR, "premake5.lua")
REASON_TRACE_GOVERNANCE_MD = os.path.join(ROOT_DIR, ".monkeycode", "specs", "gc-refactor-next-steps", "reason-trace-governance.md")
GC_TUS = sorted(glob.glob(os.path.join(ROOT_DIR, "dll", "gbe_dota_*.cpp"))) + [MAIN_CPP]
TEMPLATE_BLOB_OWNER_FILES = {
    "gbe_dota_template_replay_handlers.cpp",
    "gbe_dota_gc_payload_helpers.cpp",
}
SOURCE_LIST_AUDIT_EXEMPTIONS = {
    "gbe_dota_chat_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_connection_lifecycle.cpp": "production lifecycle TU, not directly offline-buildable",
    "gbe_dota_gc_payload_helpers.cpp": "compiled through payload helper test wrapper",
    "gbe_dota_inventory_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_inventory_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_flow_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_launch_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_snapshot_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_state_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_match_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_misc_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_network_callbacks.cpp": "production callback TU, not directly offline-buildable",
    "gbe_dota_payload_item_helpers.cpp": "compiled through test wrappers",
    "gbe_dota_payload_lobby_helpers.cpp": "compiled through payload helper test wrapper",
    "gbe_dota_payload_wire_helpers.cpp": "compiled through payload helper test wrapper",
    "gbe_dota_post_login_handlers.cpp": "production dispatcher TU, covered by registry audit",
    "gbe_dota_template_replay_handlers.cpp": "production template replay TU with canned payload ownership",
    "gbe_dota_welcome_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
}
HIGH_RISK_SIDE_EFFECT_APIS = [
    "save_items_to_file",
    "GBE_SaveDotaItemsFromExecutor",
    "GBE_PushDotaPlayerEquippedItemsCacheToGC",
    "push_incoming_message",
    "sendToAllGameservers",
    "GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot",
    "GBE_PublishSharedDotaLobbyState",
    "GBE_PublishDotaPracticeLobbyMetadata",
    "GBE_PublishDotaPracticeLobbyLocalMemberData",
]
HIGH_RISK_SIDE_EFFECT_HANDLER_BASELINE = {
    ("gbe_dota_chat_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 5,
    ("gbe_dota_inventory_handlers.cpp", "GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot"): 1,
    ("gbe_dota_inventory_handlers.cpp", "GBE_PushDotaPlayerEquippedItemsCacheToGC"): 2,
    ("gbe_dota_inventory_handlers.cpp", "GBE_SaveDotaItemsFromExecutor"): 1,
    ("gbe_dota_inventory_handlers.cpp", "push_incoming_message"): 2,
    ("gbe_dota_inventory_handlers.cpp", "save_items_to_file"): 2,
    ("gbe_dota_inventory_handlers.cpp", "sendToAllGameservers"): 1,
    ("gbe_dota_lobby_handlers.cpp", "GBE_PublishDotaPracticeLobbyLocalMemberData"): 4,
    ("gbe_dota_lobby_handlers.cpp", "GBE_PublishDotaPracticeLobbyMetadata"): 2,
    ("gbe_dota_lobby_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 7,
    ("gbe_dota_match_handlers.cpp", "GBE_PublishDotaPracticeLobbyLocalMemberData"): 1,
    ("gbe_dota_match_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 7,
    ("gbe_dota_match_handlers.cpp", "GBE_PushDotaPlayerEquippedItemsCacheToGC"): 2,
    ("gbe_dota_misc_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_post_login_handlers.cpp", "GBE_PublishDotaPracticeLobbyMetadata"): 1,
    ("gbe_dota_post_login_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 3,
    ("gbe_dota_post_login_handlers.cpp", "save_items_to_file"): 1,
    ("gbe_dota_template_replay_handlers.cpp", "save_items_to_file"): 4,
}
HIGH_RISK_REASON_STRINGS = [
    "equip_forward_host_resubscribe_server",
    "equip_items_refresh",
    "7272_7014",
    "7272_leave_chat",
    "7035_current_game_disconnect",
    "postgame_teardown_7014",
    "7034_connected_player",
    "7034_disconnected_player",
    "runtime AP hero_selection fallback strategy_time",
    "7034_launch_poll",
    "7070_custom_game_ready_up_run_ack",
    "8052_started_loading",
    "8053_finished_loading",
    "8053_load_failed",
]

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

POST_LOGIN_DIRECT_DISPATCH_ENTRIES = [
    ("7427u", "adapt_direct_7427_notifications", "GBE_HandleDota7427NotificationsRequest"),
    ("4523u", "adapt_direct_upload_rate", "GBE_HandleDotaUploadRateRequest"),
    ("8879u", "adapt_direct_rank", "GBE_HandleDotaRankRequest"),
    ("7534u", "adapt_direct_profile_card", "GBE_HandleDotaProfileCardRequest"),
    ("2581u", "adapt_direct_lookup_account_name", "GBE_HandleDotaLookupAccountNameRequest"),
    ("7503u", "adapt_direct_emoticon_data", "GBE_HandleDotaEmoticonDataRequest"),
    ("8095u", "adapt_direct_conduct_scorecard", "GBE_HandleDotaConductScorecardRequest"),
    ("8800u", "adapt_direct_coaching_summary", "GBE_HandleDotaCoachingSummaryRequest"),
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

    for emsg, adapter, handler in POST_LOGIN_DIRECT_DISPATCH_ENTRIES:
        table_pattern = re.compile(r"\{\s*" + re.escape(emsg) + r"\s*,\s*" + re.escape(adapter) + r"\s*\}")
        if not table_pattern.search(dispatch_text):
            issues.append(f"{emsg}: missing direct-only table entry for {adapter}")

        adapter_pattern = re.compile(
            r"auto\s+" + re.escape(adapter) +
            r"\s*=.*?DotaGcRequestPath::Direct.*?return\s+self->" + re.escape(handler) + r"\s*\(",
            re.DOTALL,
        )
        if not adapter_pattern.search(dispatch_text):
            issues.append(f"{adapter}: missing direct path guard or call to {handler}")

    found_entries = re.findall(r"\{\s*(GBE_k[A-Za-z0-9_]+|\d+u)\s*,\s*(adapt_[A-Za-z0-9_]+)\s*\}", dispatch_text)
    expected_pairs = {(emsg, adapter) for emsg, adapter, _ in POST_LOGIN_DISPATCH_ENTRIES}
    expected_pairs.update((emsg, adapter) for emsg, adapter, _ in POST_LOGIN_DIRECT_DISPATCH_ENTRIES)
    found_pairs = set(found_entries)
    for emsg, adapter in sorted(found_pairs - expected_pairs):
        issues.append(f"{emsg}: unexpected dispatch table adapter {adapter}")

    return issues


def audit_template_blob_ownership(tu_paths):
    """Keep large canned template/replay blobs out of ordinary handlers."""
    issues = []
    long_hex_literal = re.compile(r'"[0-9a-fA-F]{80,}"')
    for path in tu_paths:
        base = os.path.basename(path)
        if base in TEMPLATE_BLOB_OWNER_FILES:
            continue
        if not base.startswith("gbe_dota_") or not base.endswith("_handlers.cpp"):
            continue
        for line_no, line in enumerate(read(path).splitlines(), 1):
            if long_hex_literal.search(line):
                issues.append((base, line_no, "large hex literal"))
    return issues


def audit_source_list_inclusion(tu_paths):
    """Ensure testable split GC TUs stay in shell or Premake test source lists."""
    shell_text = read(RUN_GC_OFFLINE_TESTS_SH) if os.path.exists(RUN_GC_OFFLINE_TESTS_SH) else ""
    premake_text = read(PREMAKE5_LUA) if os.path.exists(PREMAKE5_LUA) else ""
    issues = []
    checked = 0
    exempted = []

    for path in sorted(tu_paths):
        base = os.path.basename(path)
        if not base.startswith("gbe_dota_") or not base.endswith(".cpp"):
            continue

        rel = "dll/" + base
        in_shell = rel in shell_text
        in_premake = rel in premake_text
        if in_shell or in_premake:
            checked += 1
            continue

        reason = SOURCE_LIST_AUDIT_EXEMPTIONS.get(base)
        if reason:
            exempted.append((base, reason))
            continue

        issues.append(f"{base}: missing from run_gc_offline_tests.sh and premake5.lua GC test source lists, with no audit exemption")

    return issues, checked, exempted


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def audit_handler_side_effect_seams(tu_paths):
    """Detect drift in high-risk side effects in ordinary handler files."""
    pattern = re.compile(r"\b(" + "|".join(re.escape(api) for api in HIGH_RISK_SIDE_EFFECT_APIS) + r")\s*\(")
    actual = {}
    for path in tu_paths:
        base = os.path.basename(path)
        if not base.startswith("gbe_dota_") or not base.endswith("_handlers.cpp"):
            continue
        text = strip_comments(read(path))
        for match in pattern.finditer(text):
            line_start = text.rfind("\n", 0, match.start()) + 1
            line = text[line_start:match.start()].strip()
            if "::" in line:
                continue
            key = (base, match.group(1))
            actual[key] = actual.get(key, 0) + 1

    issues = []
    for key, actual_count in sorted(actual.items()):
        expected_count = HIGH_RISK_SIDE_EFFECT_HANDLER_BASELINE.get(key)
        if expected_count is None:
            issues.append(f"{key[0]}: new high-risk side-effect call to {key[1]} requires an approved seam or explicit baseline entry")
        elif actual_count != expected_count:
            issues.append(f"{key[0]}: {key[1]} count changed from {expected_count} to {actual_count}; route through an approved seam or update the baseline with reason")

    for key, expected_count in sorted(HIGH_RISK_SIDE_EFFECT_HANDLER_BASELINE.items()):
        actual_count = actual.get(key, 0)
        if actual_count == 0 and expected_count:
            issues.append(f"{key[0]}: {key[1]} baseline expected {expected_count}, found 0; remove stale baseline entry or confirm the seam migration")

    return issues, sum(actual.values()), len(HIGH_RISK_SIDE_EFFECT_HANDLER_BASELINE)


def audit_reason_inventory():
    """Ensure high-risk reasons stay documented and covered by tests/specs."""
    governance_text = read(REASON_TRACE_GOVERNANCE_MD) if os.path.exists(REASON_TRACE_GOVERNANCE_MD) else ""
    coverage_text = ""
    for pattern in (
        os.path.join(ROOT_DIR, "tools", "*.cpp"),
        os.path.join(ROOT_DIR, "tools", "*", "*.cpp"),
        os.path.join(ROOT_DIR, ".monkeycode", "specs", "gc-refactor-next-steps", "*.md"),
        os.path.join(ROOT_DIR, ".monkeycode", "specs", "gc-refactor-follow-up", "*.md"),
    ):
        for path in glob.glob(pattern):
            coverage_text += "\n" + read(path)

    issues = []
    for reason in HIGH_RISK_REASON_STRINGS:
        if f"`{reason}`" not in governance_text:
            issues.append(f"{reason}: missing from reason-trace-governance.md high-risk inventory")
        if reason not in coverage_text:
            issues.append(f"{reason}: missing from focused tests or specs coverage text")
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
        total_entries = len(POST_LOGIN_DISPATCH_ENTRIES) + len(POST_LOGIN_DIRECT_DISPATCH_ENTRIES)
        print(f"  All {total_entries} dispatch entries map to the expected handlers")
    else:
        for issue in dispatch_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 5: Template/replay canned blob ownership")
    print("=" * 70)
    print("  Action: keep large canned template/replay hex in the template replay or payload helper owners.")
    template_blob_issues = audit_template_blob_ownership(GC_TUS)
    if not template_blob_issues:
        print("  (none) - ordinary handler files do not define large canned hex literals")
    else:
        for f, ln, reason in template_blob_issues:
            print(f"  {f}:{ln}: {reason}; move canned data behind the template/replay or payload helper boundary")
    print()

    print("=" * 70)
    print("AUDIT 6: GC source-list inclusion")
    print("=" * 70)
    print("  Action: add testable split GC TUs to shell/Premake test source lists or record an explicit exemption.")
    source_list_issues, source_list_checked, source_list_exempted = audit_source_list_inclusion(GC_TUS)
    if not source_list_issues:
        print(f"  All {source_list_checked} testable split GC TUs appear in shell or Premake test source lists")
        if source_list_exempted:
            print(f"  {len(source_list_exempted)} production-only or wrapper-compiled TUs have explicit audit exemptions")
    else:
        for issue in source_list_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 7: Handler side-effect seam drift")
    print("=" * 70)
    print("  Action: keep high-risk handler side effects behind approved seams or explicit baselines.")
    side_effect_issues, side_effect_calls, side_effect_baselines = audit_handler_side_effect_seams(GC_TUS)
    if not side_effect_issues:
        print(f"  All {side_effect_calls} high-risk handler side-effect calls match {side_effect_baselines} explicit baseline entries")
    else:
        for issue in side_effect_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 8: High-risk reason inventory")
    print("=" * 70)
    print("  Action: document high-risk reason strings and keep matching test/spec coverage text.")
    reason_issues = audit_reason_inventory()
    if not reason_issues:
        print(f"  All {len(HIGH_RISK_REASON_STRINGS)} high-risk reason strings are documented and covered")
    else:
        for issue in reason_issues:
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
    print(f"  Template blob ownership issues:      {len(template_blob_issues)}")
    print(f"  Source-list inclusion issues:        {len(source_list_issues)}")
    print(f"  Handler side-effect seam issues:     {len(side_effect_issues)}")
    print(f"  High-risk reason inventory issues:   {len(reason_issues)}")

    if zombies or underexposed or mismatches or dispatch_issues or template_blob_issues or source_list_issues or side_effect_issues or reason_issues:
        sys.exit(1)


if __name__ == "__main__":
    main()
