#!/usr/bin/env python3
"""Comprehensive audit of the GC refactor.

Checks:
  1. Every GBE_* declaration in gbe_dota_gc_internal.h has a matching
     definition somewhere in the GC TUs (find zombie declarations).
  2. Every shared GBE_* free-function definition has a declaration in the
     header, while member/static helpers are classified as non-actionable.
  3. Doc line numbers in REFACTOR_TODO.md match actual code.
  4. Production code cannot access the retired mutable shared lobby global.
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
HANDLER_SMOKE_CPP = os.path.join(ROOT_DIR, "tools", "gbe_dota_handler_test", "smoke_test.cpp")
REPLAY_FIXTURE_DIR = os.path.join(ROOT_DIR, "tools", "gc_replay_test", "fixtures")
TODO_MD = os.path.join(ROOT_DIR, "REFACTOR_TODO.md")
RUN_GC_OFFLINE_TESTS_SH = os.path.join(ROOT_DIR, "tools", "run_gc_offline_tests.sh")
PREMAKE5_LUA = os.path.join(ROOT_DIR, "premake5.lua")
REASON_TRACE_GOVERNANCE_MD = os.path.join(ROOT_DIR, "docs", "gc", "reason-trace-governance.md")
DIAGNOSTIC_EVENT_H = os.path.join(ROOT_DIR, "dll", "gbe_dota_diagnostic_event.h")
DIAGNOSTIC_EVENT_TEST_CPP = os.path.join(
    ROOT_DIR,
    "tools",
    "gbe_dota_reconnect_network_test",
    "gbe_dota_reconnect_network_test.cpp",
)
GC_TUS = sorted(glob.glob(os.path.join(ROOT_DIR, "dll", "gbe_dota_*.cpp"))) + [MAIN_CPP]
TEMPLATE_BLOB_OWNER_FILES = {
    "gbe_dota_template_replay_handlers.cpp",
    "gbe_dota_gc_payload_helpers.cpp",
}
SOURCE_LIST_AUDIT_EXEMPTIONS = {
    "gbe_dota_chat_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_connection_lifecycle.cpp": "production lifecycle TU, not directly offline-buildable",
    "gbe_dota_custom_game_lifecycle_coordinator.cpp": "production coordinator TU, compiled through handler test wrapper",
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
    "gbe_dota_reconnect_network_adapter.cpp": "production Steam networking adapter, covered through the reconnect network boundary",
    "gbe_dota_post_login_handlers.cpp": "production dispatcher TU, covered by registry audit",
    "gbe_dota_template_replay_handlers.cpp": "production template replay TU with canned payload ownership",
    "gbe_dota_welcome_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_wrapped_custom_game_handlers.cpp": "compiled through handler test wrapper",
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
    "GBE_SetDotaLobbyMemberRuntimeState",
    "GBE_TryQueueDotaRuntimeLobbyDetailsUpdate",
    "GBE_MarkDotaLaunchPhase",
    "GBE_SendDotaPracticeLobbyDetailsUpdate",
]
HIGH_RISK_SIDE_EFFECT_HANDLER_BASELINE = {
    ("gbe_dota_chat_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 5,
    ("gbe_dota_chat_handlers.cpp", "GBE_SendDotaPracticeLobbyDetailsUpdate"): 3,
    ("gbe_dota_inventory_handlers.cpp", "GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot"): 1,
    ("gbe_dota_inventory_handlers.cpp", "GBE_PushDotaPlayerEquippedItemsCacheToGC"): 2,
    ("gbe_dota_inventory_handlers.cpp", "GBE_SaveDotaItemsFromExecutor"): 1,
    ("gbe_dota_inventory_handlers.cpp", "push_incoming_message"): 2,
    ("gbe_dota_inventory_handlers.cpp", "save_items_to_file"): 2,
    ("gbe_dota_inventory_handlers.cpp", "sendToAllGameservers"): 1,
    ("gbe_dota_lobby_handlers.cpp", "GBE_PublishDotaPracticeLobbyLocalMemberData"): 4,
    ("gbe_dota_lobby_handlers.cpp", "GBE_PublishDotaPracticeLobbyMetadata"): 2,
    ("gbe_dota_lobby_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 7,
    ("gbe_dota_lobby_handlers.cpp", "GBE_SendDotaPracticeLobbyDetailsUpdate"): 5,
    ("gbe_dota_match_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_match_handlers.cpp", "GBE_PushDotaPlayerEquippedItemsCacheToGC"): 2,
    ("gbe_dota_misc_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_misc_handlers.cpp", "GBE_SendDotaPracticeLobbyDetailsUpdate"): 1,
    ("gbe_dota_post_login_handlers.cpp", "GBE_MarkDotaLaunchPhase"): 1,
    ("gbe_dota_post_login_handlers.cpp", "GBE_PublishDotaPracticeLobbyMetadata"): 1,
    ("gbe_dota_post_login_handlers.cpp", "save_items_to_file"): 1,
    ("gbe_dota_template_replay_handlers.cpp", "save_items_to_file"): 4,
}
LIFECYCLE_EXECUTOR_OWNER = "gbe_dota_custom_game_lifecycle_coordinator.cpp"
LIFECYCLE_PLANNER = "gbe_dota_lifecycle_actions.cpp"
LIFECYCLE_SIDE_EFFECT_APIS = [
    "GBE_SetDotaLobbyMemberRuntimeState",
    "GBE_TryQueueDotaRuntimeLobbyDetailsUpdate",
    "GBE_MarkDotaLaunchPhase",
    "GBE_PublishDotaPracticeLobbyLocalMemberData",
    "GBE_PublishSharedDotaLobbyState",
    "GBE_SendDotaPracticeLobbyDetailsUpdate",
]
MIGRATED_LIFECYCLE_HANDLER_FORBIDDEN_APIS = {
    "gbe_dota_match_handlers.cpp": {
        "GBE_SetDotaLobbyMemberRuntimeState",
        "GBE_TryQueueDotaRuntimeLobbyDetailsUpdate",
        "GBE_MarkDotaLaunchPhase",
        "GBE_PublishDotaPracticeLobbyLocalMemberData",
        "GBE_SendDotaPracticeLobbyDetailsUpdate",
    },
    "gbe_dota_wrapped_custom_game_handlers.cpp": set(LIFECYCLE_SIDE_EFFECT_APIS),
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
RETIRED_SHARED_LOBBY_GLOBAL = "GBE_shared_dota_lobby_state"


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
        return ["GBE_DispatchDotaPostLoginRequest definition not found"], 0
    end = main_text.find("bool Steam_Game_Coordinator::gc_enabled", start)
    dispatch_text = main_text[start:end if end >= 0 else len(main_text)]

    table_start = dispatch_text.find("static const registry::Entry kTable[]")
    table_end = dispatch_text.find("};", table_start)
    if table_start < 0 or table_end < 0:
        return ["typed post-login registry table not found"], 0
    table_text = dispatch_text[table_start:table_end]

    entry_pattern = re.compile(
        r"\{\s*(GBE_k[A-Za-z0-9_]+|\d+u)\s*,\s*"
        r"registry::RequestMode::([A-Za-z]+)\s*,\s*"
        r"registry::SessionPolicy::([A-Za-z]+)\s*,\s*"
        r"registry::LifecycleClass::([A-Za-z]+)\s*,\s*"
        r"(adapt_[A-Za-z0-9_]+)\s*,\s*"
        r"registry::HandlerId::([A-Za-z0-9_]+)\s*,\s*"
        r"(nullptr|\"[^\"]+\")\s*\}",
    )
    entries = entry_pattern.findall(table_text)

    adapter_pattern = re.compile(
        r"auto\s+(adapt_[A-Za-z0-9_]+)\s*=\s*\+\[\]\(.*?\)\s*->\s*bool\s*\{(.*?)\n\s*\};",
        re.DOTALL,
    )
    adapters = dict(adapter_pattern.findall(dispatch_text[:table_start]))

    issues = []
    raw_entry_count = len(re.findall(r"^\s*\{", table_text, re.MULTILINE))
    if len(entries) != raw_entry_count:
        issues.append(f"registry parse covered {len(entries)} of {raw_entry_count} entries")

    smoke_text = read(HANDLER_SMOKE_CPP)
    smoke_tests = set(re.findall(r"^static void (test_[A-Za-z0-9_]+)\(\)", smoke_text, re.MULTILINE))
    replay_labels = {}
    for fixture_path in glob.glob(os.path.join(REPLAY_FIXTURE_DIR, "*.txt")):
        if fixture_path.endswith(".expected.txt"):
            continue
        fixture_name = os.path.basename(fixture_path)[:-4]
        labels = set()
        for line in read(fixture_path).splitlines():
            fields = line.split(maxsplit=2)
            if len(fields) == 3:
                labels.add(fields[2])
        replay_labels[fixture_name] = labels

    registered_adapters = set()
    high_risk_entries = 0
    for emsg, mode, session_policy, lifecycle, adapter, handler_id, fixture_literal in entries:
        registered_adapters.add(adapter)
        body = adapters.get(adapter)
        if body is None:
            issues.append(f"{emsg}: registry adapter {adapter} has no lambda definition")
            continue

        handler_calls = re.findall(r"self->(GBE_HandleDota[A-Za-z0-9_]+Request)\s*\(", body)
        if len(handler_calls) != 1:
            issues.append(f"{emsg}: {adapter} calls {len(handler_calls)} request handlers")

        has_direct_guard = "DotaGcRequestPath::Direct" in body
        if mode == "Direct" and not has_direct_guard:
            issues.append(f"{emsg}: direct-only adapter {adapter} has no direct path guard")
        if mode == "DirectAndWrapped" and has_direct_guard:
            issues.append(f"{emsg}: dual-mode adapter {adapter} contains a direct-only path guard")

        if mode == "Direct" and session_policy != "Ignore":
            issues.append(f"{emsg}: direct-only entry forwards wrapped session metadata")
        if handler_id == "Unknown":
            issues.append(f"{emsg}: registry entry has unknown handler identity")
        if lifecycle not in {"None", "LobbyRead", "LobbyMutation", "LobbyLifecycle"}:
            issues.append(f"{emsg}: registry entry has unknown lifecycle class {lifecycle}")

        fixture = None if fixture_literal == "nullptr" else fixture_literal[1:-1]
        if lifecycle in {"LobbyMutation", "LobbyLifecycle"}:
            high_risk_entries += 1
            if fixture is None:
                issues.append(f"{emsg}: high-risk {lifecycle} entry has no smoke/replay fixture")
                continue
        if fixture is None:
            continue
        fixture_parts = fixture.split(":")
        if len(fixture_parts) == 2 and fixture_parts[0] == "smoke":
            if fixture_parts[1] not in smoke_tests:
                issues.append(f"{emsg}: smoke fixture {fixture_parts[1]} does not exist")
        elif len(fixture_parts) == 3 and fixture_parts[0] == "replay":
            fixture_name, label = fixture_parts[1], fixture_parts[2]
            if fixture_name not in replay_labels:
                issues.append(f"{emsg}: replay fixture file {fixture_name}.txt does not exist")
            elif label not in replay_labels[fixture_name]:
                issues.append(f"{emsg}: replay label {label} does not exist in {fixture_name}.txt")
        else:
            issues.append(f"{emsg}: fixture {fixture} must use smoke:<test> or replay:<file>:<label>")

    for adapter in sorted(set(adapters) - registered_adapters):
        issues.append(f"{adapter}: adapter lambda is not referenced by the typed registry")

    return issues, len(entries), high_risk_entries


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


def audit_lifecycle_side_effect_ownership():
    """Keep lifecycle planning pure and migrated handlers behind the executor."""
    issues = []
    owner_path = os.path.join(ROOT_DIR, "dll", LIFECYCLE_EXECUTOR_OWNER)
    owner_text = strip_comments(read(owner_path))
    for api in LIFECYCLE_SIDE_EFFECT_APIS:
        if not re.search(r"\b" + re.escape(api) + r"\s*\(", owner_text):
            issues.append(f"{LIFECYCLE_EXECUTOR_OWNER}: executor owner no longer calls required lifecycle API {api}")

    planner_path = os.path.join(ROOT_DIR, "dll", LIFECYCLE_PLANNER)
    planner_text = strip_comments(read(planner_path))
    for api in LIFECYCLE_SIDE_EFFECT_APIS:
        if re.search(r"\b" + re.escape(api) + r"\s*\(", planner_text):
            issues.append(f"{LIFECYCLE_PLANNER}: pure planner directly calls lifecycle side-effect API {api}")

    for base, forbidden_apis in sorted(MIGRATED_LIFECYCLE_HANDLER_FORBIDDEN_APIS.items()):
        handler_text = strip_comments(read(os.path.join(ROOT_DIR, "dll", base)))
        for api in sorted(forbidden_apis):
            if re.search(r"\b" + re.escape(api) + r"\s*\(", handler_text):
                issues.append(f"{base}: migrated lifecycle handler directly calls {api}; route through {LIFECYCLE_EXECUTOR_OWNER}")

    return issues


def extract_diagnostic_reason_inventory(header_text):
    """Derive typed diagnostic reason names and stable serialized values."""
    enum_match = re.search(r"enum\s+class\s+Reason\s*:[^{]+\{(?P<body>.*?)\};", header_text, re.S)
    if not enum_match:
        return [], {}, ["Reason enum: missing from gbe_dota_diagnostic_event.h"]

    enum_names = []
    for entry in enum_match.group("body").split(","):
        name = entry.split("//", 1)[0].strip()
        if name:
            enum_names.append(name.split("=", 1)[0].strip())

    describe_match = re.search(
        r"describe_reason\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        header_text,
        re.S,
    )
    if not describe_match:
        return enum_names, {}, ["describe_reason: missing from gbe_dota_diagnostic_event.h"]

    mappings = dict(re.findall(
        r'case\s+Reason::(\w+)\s*:\s*return\s+"([^"]*)"\s*;',
        describe_match.group("body"),
    ))
    return enum_names, mappings, []


def audit_diagnostic_reason_inventory(header_text, focused_test_text):
    """Check the centralized typed inventory and its focused test table."""
    issues = []
    enum_names, mappings, extraction_issues = extract_diagnostic_reason_inventory(header_text)
    issues.extend(extraction_issues)

    enum_name_set = set(enum_names)
    mapping_name_set = set(mappings)
    for name in sorted(enum_name_set - mapping_name_set):
        issues.append(f"diagnostic Reason::{name}: missing stable describe_reason mapping")
    for name in sorted(mapping_name_set - enum_name_set):
        issues.append(f"diagnostic Reason::{name}: mapping has no enum entry")

    values_to_names = {}
    for name, value in mappings.items():
        values_to_names.setdefault(value, []).append(name)
    for value, names in sorted(values_to_names.items()):
        if len(names) > 1:
            issues.append(f"diagnostic reason value {value!r}: duplicate mapping for {', '.join(sorted(names))}")

    focused_mappings = dict(re.findall(
        r'\{diagnostic::Reason::(\w+),\s*"([^"]*)"\}',
        focused_test_text,
    ))
    for name in enum_names:
        expected_value = mappings.get(name)
        if name not in focused_mappings:
            issues.append(f"diagnostic Reason::{name}: missing from focused serialization inventory")
        elif expected_value is not None and focused_mappings[name] != expected_value:
            issues.append(
                f"diagnostic Reason::{name}: focused value {focused_mappings[name]!r} does not match {expected_value!r}"
            )
    for name in sorted(set(focused_mappings) - enum_name_set):
        issues.append(f"diagnostic Reason::{name}: focused inventory has no enum entry")

    return issues, len(enum_names)


def audit_reason_inventory():
    """Audit typed diagnostic reasons plus legacy high-risk reason governance."""
    governance_text = read(REASON_TRACE_GOVERNANCE_MD) if os.path.exists(REASON_TRACE_GOVERNANCE_MD) else ""
    coverage_text = ""
    for pattern in (
        os.path.join(ROOT_DIR, "tools", "*.cpp"),
        os.path.join(ROOT_DIR, "tools", "*", "*.cpp"),
        os.path.join(ROOT_DIR, "docs", "gc", "*.md"),
    ):
        for path in glob.glob(pattern):
            coverage_text += "\n" + read(path)

    issues, diagnostic_reason_count = audit_diagnostic_reason_inventory(
        read(DIAGNOSTIC_EVENT_H),
        read(DIAGNOSTIC_EVENT_TEST_CPP),
    )

    for reason in HIGH_RISK_REASON_STRINGS:
        if f"`{reason}`" not in governance_text:
            issues.append(f"{reason}: missing from reason-trace-governance.md high-risk inventory")
        if reason not in coverage_text:
            issues.append(f"{reason}: missing from focused tests or specs coverage text")
    return issues, diagnostic_reason_count, len(HIGH_RISK_REASON_STRINGS)


def audit_shared_lobby_global_access():
    """Keep production shared lobby state accessible only through the store."""
    issues = []
    paths = list(GC_TUS) + [INTERNAL_H]
    for path in paths:
        text = strip_comments(read(path))
        for match in re.finditer(r"\b" + re.escape(RETIRED_SHARED_LOBBY_GLOBAL) + r"\b", text):
            line_no = text.count("\n", 0, match.start()) + 1
            issues.append(
                f"{os.path.basename(path)}:{line_no}: direct access to {RETIRED_SHARED_LOBBY_GLOBAL}; use the shared lobby store facade"
            )
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
    dispatch_issues, total_entries, high_risk_entries = audit_post_login_dispatch(main_text)
    if not dispatch_issues:
        print(f"  All {total_entries} typed registry entries resolve to one handler adapter; {high_risk_entries} high-risk entries reference live fixtures")
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
    print("AUDIT 8: Diagnostic and high-risk reason inventory")
    print("=" * 70)
    print("  Action: derive typed reasons, reject duplicate values, and keep focused and legacy coverage aligned.")
    reason_issues, diagnostic_reason_count, high_risk_reason_count = audit_reason_inventory()
    if not reason_issues:
        print(f"  All {diagnostic_reason_count} typed diagnostic reasons have unique mappings and focused coverage")
        print(f"  All {high_risk_reason_count} legacy high-risk reason strings are documented and covered")
    else:
        for issue in reason_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 9: Lifecycle side-effect ownership")
    print("=" * 70)
    print("  Action: keep lifecycle planners pure and migrated handlers behind the lifecycle executor owner.")
    lifecycle_ownership_issues = audit_lifecycle_side_effect_ownership()
    if not lifecycle_ownership_issues:
        print(f"  All {len(LIFECYCLE_SIDE_EFFECT_APIS)} lifecycle side-effect APIs remain owned by {LIFECYCLE_EXECUTOR_OWNER}")
    else:
        for issue in lifecycle_ownership_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 10: Shared lobby global access")
    print("=" * 70)
    print("  Action: keep production shared lobby state behind the store facade.")
    shared_lobby_global_issues = audit_shared_lobby_global_access()
    if not shared_lobby_global_issues:
        print(f"  (none) - production code does not access {RETIRED_SHARED_LOBBY_GLOBAL}")
    else:
        for issue in shared_lobby_global_issues:
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
    print(f"  Lifecycle ownership issues:          {len(lifecycle_ownership_issues)}")
    print(f"  Shared lobby global access issues:   {len(shared_lobby_global_issues)}")

    if zombies or underexposed or mismatches or dispatch_issues or template_blob_issues or source_list_issues or side_effect_issues or reason_issues or lifecycle_ownership_issues or shared_lobby_global_issues:
        sys.exit(1)


if __name__ == "__main__":
    main()
