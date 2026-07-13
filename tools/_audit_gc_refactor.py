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
import json
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
INTERNAL_H = os.path.join(ROOT_DIR, "dll", "gbe_dota_gc_internal.h")
PUBLIC_HEADERS = [
    os.path.join(ROOT_DIR, "dll", "gbe_dota_payload_item_helpers.h"),
    os.path.join(ROOT_DIR, "dll", "gbe_dota_payload_lobby_helpers.h"),
    os.path.join(ROOT_DIR, "dll", "gbe_dota_payload_wire_helpers.h"),
    os.path.join(ROOT_DIR, "dll", "gbe_dota_inventory_ports.h"),
    os.path.join(ROOT_DIR, "dll", "dll", "gbe_dota_reconnect_shared.h"),
]
MAIN_CPP = os.path.join(ROOT_DIR, "dll", "steam_game_coordinator.cpp")
HANDLER_SMOKE_CPP = os.path.join(ROOT_DIR, "tools", "gbe_dota_handler_test", "smoke_test.cpp")
REPLAY_FIXTURE_DIR = os.path.join(ROOT_DIR, "tools", "gc_replay_test", "fixtures")
TODO_MD = os.path.join(ROOT_DIR, "REFACTOR_TODO.md")
RUN_GC_OFFLINE_TESTS_SH = os.path.join(ROOT_DIR, "tools", "run_gc_offline_tests.sh")
PREMAKE5_LUA = os.path.join(ROOT_DIR, "premake5.lua")
REASON_TRACE_GOVERNANCE_MD = os.path.join(ROOT_DIR, "docs", "gc", "reason-trace-governance.md")
CONCURRENCY_OWNERSHIP_MD = os.path.join(ROOT_DIR, "docs", "gc", "concurrency-ownership.md")
ARCHITECTURE_INVESTMENT_INPUTS_JSON = os.path.join(ROOT_DIR, "docs", "gc", "architecture-investment-inputs.json")
PR_WORKFLOW_YML = os.path.join(ROOT_DIR, ".github", "workflows", "emu-pull-request.yml")
DIAGNOSTIC_EVENT_H = os.path.join(ROOT_DIR, "dll", "gbe_dota_diagnostic_event.h")
DIAGNOSTIC_EVENT_TEST_CPP = os.path.join(
    ROOT_DIR,
    "tools",
    "gbe_dota_reconnect_network_test",
    "gbe_dota_reconnect_network_test.cpp",
)
GC_TUS = sorted(glob.glob(os.path.join(ROOT_DIR, "dll", "gbe_dota_*.cpp"))) + [MAIN_CPP]
MUTABLE_GC_GLOBAL_ALLOWLIST = {
    ("gbe_dota_locator.cpp", "shared_dota_lobby_store"): "non-owning compatibility locator",
    ("gbe_dota_locator.cpp", "dota_runtime_state"): "non-owning compatibility locator",
}
TEMPLATE_BLOB_OWNER_FILES = {
    "gbe_dota_template_replay_templates.cpp",
    "gbe_dota_gc_payload_helpers.cpp",
}
SOURCE_LIST_AUDIT_EXEMPTIONS = {
    "gbe_dota_chat_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_connection_lifecycle.cpp": "production lifecycle TU, not directly offline-buildable",
    "gbe_dota_custom_game_lifecycle_coordinator.cpp": "production coordinator TU, compiled through handler test wrapper",
    "gbe_dota_gc_payload_helpers.cpp": "compiled through payload helper test wrapper",
    "gbe_dota_inventory_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_inventory_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_handlers.cpp": "empty shell after D.10.1 split; real handlers in create/list/join/invite/lifecycle/slot TUs",
    "gbe_dota_lobby_handler_helpers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_create_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_list_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_join_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_invite_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_lifecycle_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_slot_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_lobby_flow_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_launch_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_snapshot_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_state_coordinator.cpp": "empty shell after D.10.2 split; real coordinators in publish/member/restore/recover TUs",
    "gbe_dota_lobby_state_publish_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_state_member_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_state_restore_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_lobby_state_recover_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_match_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_misc_handlers.cpp": "compiled through handler test wrapper",
    "gbe_dota_network_callbacks.cpp": "production callback TU, not directly offline-buildable",
    "gbe_dota_payload_item_helpers.cpp": "compiled through test wrappers",
    "gbe_dota_payload_lobby_helpers.cpp": "compiled through payload helper test wrapper",
    "gbe_dota_payload_wire_helpers.cpp": "compiled through payload helper test wrapper",
    "gbe_dota_reconnect_network_adapter.cpp": "production Steam networking adapter, covered through the reconnect network boundary",
    "gbe_dota_post_login_handlers.cpp": "production dispatcher TU, covered by registry audit",
    "gbe_dota_post_login_dispatcher.cpp": "compiled through handler test wrapper",
    "gbe_dota_template_replay_handlers.cpp": "production template replay dispatch TU; canned payloads in templates TU",
    "gbe_dota_template_replay_templates.cpp": "production template-replay protocol asset TU (D.12.1)",
    "gbe_dota_welcome_coordinator.cpp": "production coordinator TU, not directly offline-buildable",
    "gbe_dota_custom_game_lifecycle_handlers.cpp": "compiled through handler test wrapper",
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
    ("gbe_dota_lobby_create_handlers.cpp", "GBE_PublishDotaPracticeLobbyLocalMemberData"): 2,
    ("gbe_dota_lobby_create_handlers.cpp", "GBE_PublishDotaPracticeLobbyMetadata"): 2,
    ("gbe_dota_lobby_create_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_lobby_create_handlers.cpp", "GBE_SendDotaPracticeLobbyDetailsUpdate"): 1,
    ("gbe_dota_lobby_join_handlers.cpp", "GBE_PublishDotaPracticeLobbyLocalMemberData"): 1,
    ("gbe_dota_lobby_join_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 1,
    ("gbe_dota_lobby_lifecycle_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_lobby_lifecycle_handlers.cpp", "GBE_SendDotaPracticeLobbyDetailsUpdate"): 2,
    ("gbe_dota_lobby_slot_handlers.cpp", "GBE_PublishDotaPracticeLobbyLocalMemberData"): 1,
    ("gbe_dota_lobby_slot_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_lobby_slot_handlers.cpp", "GBE_SendDotaPracticeLobbyDetailsUpdate"): 2,
    ("gbe_dota_match_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_misc_handlers.cpp", "GBE_PublishSharedDotaLobbyState"): 2,
    ("gbe_dota_misc_handlers.cpp", "GBE_SendDotaPracticeLobbyDetailsUpdate"): 1,
    ("gbe_dota_post_login_handlers.cpp", "GBE_MarkDotaLaunchPhase"): 1,
    ("gbe_dota_post_login_handlers.cpp", "GBE_PublishDotaPracticeLobbyMetadata"): 1,
    ("gbe_dota_post_login_handlers.cpp", "save_items_to_file"): 1,
    ("gbe_dota_template_replay_handlers.cpp", "save_items_to_file"): 4,
}
HANDLER_RESPONSIBILITY_BASELINE = {
    ("gbe_dota_chat_handlers.cpp", "GBE_local_lobby assignment"): 1,
    ("gbe_dota_chat_handlers.cpp", "push_incoming_now"): 1,
    ("gbe_dota_custom_game_lifecycle_handlers.cpp", "push_incoming_now"): 1,
    ("gbe_dota_inventory_handlers.cpp", "network sendToAllGameservers"): 1,
    ("gbe_dota_inventory_handlers.cpp", "push_incoming_now"): 6,
    ("gbe_dota_lobby_create_handlers.cpp", "GBE_local_lobby assignment"): 1,
    ("gbe_dota_lobby_create_handlers.cpp", "push_incoming_now"): 1,
    ("gbe_dota_lobby_join_handlers.cpp", "GBE_local_lobby assignment"): 1,
    ("gbe_dota_lobby_join_handlers.cpp", "push_incoming_now"): 2,
    ("gbe_dota_lobby_invite_handlers.cpp", "push_incoming_now"): 2,
    ("gbe_dota_lobby_lifecycle_handlers.cpp", "GBE_local_lobby assignment"): 1,
    ("gbe_dota_lobby_lifecycle_handlers.cpp", "push_incoming_now"): 2,
    ("gbe_dota_lobby_slot_handlers.cpp", "GBE_local_lobby assignment"): 1,
    ("gbe_dota_match_handlers.cpp", "push_incoming_now"): 1,
    ("gbe_dota_misc_handlers.cpp", "push_incoming_now"): 1,
    # 1x AddSocket + 2x WatchGame direct (pending/ready; moved from template in B1)
    ("gbe_dota_post_login_handlers.cpp", "push_incoming_now"): 3,
    ("gbe_dota_template_replay_handlers.cpp", "push_incoming_now"): 10,
}
PURE_DOTA_PLANNER_FILES = (
    "gbe_dota_lifecycle_actions.cpp",
    "gbe_dota_lobby_flow.cpp",
    "gbe_dota_lobby_launch_flow.cpp",
    "gbe_dota_lobby_member_flow.cpp",
    "gbe_dota_lobby_payload_flow.cpp",
    "gbe_dota_chat_flow.cpp",
)
PURE_DOTA_PLANNER_FORBIDDEN_TOKENS = (
    "push_incoming_now(",
    "sendToAllGameservers(",
    "ConnectByIPAddress(",
    "addCBResult(",
    "GBE_GetSharedDotaLobbyStateStore(",
    "GBE_SharedLobbyStore(",
    "GBE_PublishSharedDotaLobbyState(",
    "GBE_SendDotaPracticeLobbyDetailsUpdate(",
)
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
    "gbe_dota_custom_game_lifecycle_handlers.cpp": set(LIFECYCLE_SIDE_EFFECT_APIS),
}
RETIRED_LIFECYCLE_HANDLER_SYMBOLS = (
    "GBE_HandleDotaCustomGameReadyUpRequest",
    "GBE_HandleDotaCustomGameStartedLoadingRequest",
    "GBE_HandleDotaCustomGameFinishedLoadingRequest",
    "GBE_HandleDotaWrappedCustomGameLifecycleRequest",
)
RETIRED_LIFECYCLE_FALLBACK_EMSGS = ("7070u", "8052u", "8053u")
LIFECYCLE_TRANSITION_GATES = {
    "gbe_dota_custom_game_lifecycle_handlers.cpp": (
        "transition_custom_game_request(",
        "EffectKind::CustomGameLifecycleActionsRequested",
    ),
    "gbe_dota_match_handlers.cpp": (
        "transition_runtime_member(",
        "EffectKind::RuntimeMemberUpdateRequested",
        "transition_runtime_game_state(",
        "EffectKind::RuntimeGameStateUpdateRequested",
        "transition_runtime_poll(",
        "EffectKind::PracticeLobbyDetailsRequested",
    ),
    "gbe_dota_lobby_lifecycle_handlers.cpp": (
        "transition_teardown(",
        "EffectKind::TeardownAbandonInitiateRequested",
        "EffectKind::TeardownPostGameInitiateRequested",
        "EffectKind::TeardownLeaveInitiateRequested",
        "abandon_initiate_preflight_action_list(",
    ),
    "gbe_dota_lobby_list_handlers.cpp": (
        "transition_teardown(",
        "EffectKind::TeardownLeaveFinalizeRequested",
        "leave_lobby_finalize_action_list(",
    ),
    "gbe_dota_lobby_flow_coordinator.cpp": (
        "transition_teardown(",
        "EffectKind::TeardownAbandonFinalizeRequested",
        "EffectKind::TeardownPostGameFinalizeRequested",
    ),
    "gbe_dota_lobby_state_member_coordinator.cpp": (
        "transition_teardown(",
        "EffectKind::TeardownPostGameInitiateRequested",
    ),
}
RETIRED_RECONNECT_TRANSITION_SYMBOLS = (
    "GBE_DotaReconnectSharedStateSnapshot",
    "GBE_GetSharedDotaReconnectStateSnapshot",
    "source_from_shared_snapshot",
    "build_reconnect_context",
    "describe_source_kind",
    "describe_reject_reason",
    "GBE_DescribeDotaReconnectPostSkipReason",
)
GENERATION_COUNTER_OWNER = "steam_game_coordinator.cpp"
GENERATION_COUNTER_SYNC_OWNER = "gbe_dota_lobby_state_restore_coordinator.cpp"
GENERATION_COUNTER_SYNC_BASELINE = 2
RECONNECT_TRANSITION_OWNER = "gbe_dota_reconnect_network.cpp"
CORE_STATE_MACHINE_HEADER_TOKENS = (
    "constexpr MachineTransitionResult transition(MachineState state, const Event &event)",
    "constexpr MachineTransitionResult transition_runtime_poll(",
    "constexpr MachineTransitionResult transition_custom_game_request(",
    "constexpr MachineTransitionResult transition_runtime_member(",
    "constexpr MachineTransitionResult transition_runtime_game_state(",
    "constexpr MachineTransitionResult transition_teardown(",
)
ASYNC_GENERATION_GATES = {
    "steam_game_coordinator.cpp": (
        "new_item.lobby_id = GBE_local_lobby.lobby_id;",
        "new_item.generation = GBE_CurrentDotaLobbyGeneration();",
        'GBE_IsQueuedLobbyMessageCurrent(new_item, "enqueue_immediate")',
        'GBE_IsQueuedLobbyMessageCurrent(*it, "delay_expired")',
        'GBE_IsQueuedLobbyMessageCurrent(*it, "before_incoming_queue")',
        "slot.generation == GBE_CurrentDotaLobbyGeneration()",
    ),
    "gbe_dota_reconnect_network.cpp": (
        "callback_queue.queue_game_server_change(plan.server_change, 0.0, result.context.generation)",
    ),
    "gbe_dota_reconnect_network_adapter.cpp": (
        "current_context.generation == expected_generation",
        "SteamCallExecutionGuard(",
        "&generation,",
        "sizeof(generation)",
    ),
    "gbe_dota_match_handlers.cpp": (
        "transition_runtime_member(",
        "transition_runtime_game_state(",
        "transition_runtime_poll(",
        "machine_state.generation = GBE_CurrentDotaLobbyGeneration();",
    ),
}
ASYNC_GENERATION_REGRESSION_TESTS = (
    "test_lobby_fast_leave_rejoin_same_id_rejects_old_generation_work",
    "test_lobby_stale_postgame_task_is_rejected",
    "test_lobby_stale_delayed_runtime_task_is_rejected",
)
TEST_CREDIBILITY_GATES = {
    "tools/gbe_dota_lifecycle_state_machine_test/gbe_dota_lifecycle_state_machine_test.cpp": (
        "run_normal_launch_example",
        "run_reconnect_example",
        "run_state_machine_properties",
        "run_model_based_differential_test",
    ),
    "tools/gbe_dota_handler_test/smoke_test.cpp": (
        "test_custom_game_lifecycle_direct_wrapped_action_sequence_equivalence",
        "test_lifecycle_executor_empty_and_conditional_actions",
        "test_lifecycle_executor_push_routes_and_failure_policy",
    ),
    "tools/gbe_dota_reconnect_network_test/gbe_dota_reconnect_network_test.cpp": (
        "test_prepare_reserves_state_before_unlocked_effects",
        "test_dedup_and_generation_changes",
        "test_properties",
        "test_concurrency_properties",
    ),
    "tools/gbe_dota_composition_root_test/gbe_dota_composition_root_test.cpp": (
        "test_client_assembly_uses_client_dependencies_only",
        "test_gameserver_assembly_uses_gameserver_dependencies_only",
        "test_offline_fake_assemblies_are_isolated",
    ),
}
TEST_CREDIBILITY_RUNNER_TARGETS = (
    "gbe_dota_reconnect_network_test",
    "gbe_dota_composition_root_test",
    "gbe_dota_lifecycle_state_machine_test",
    "gbe_dota_handler_test",
)
CI_LOCALIZATION_GATES = {
    "emu-win-release": ('name: "win"', "emu-build-all-win.yml", "continue_on_error: false"),
    "emu-linux-release": ('name: "linux"', "emu-build-all-linux.yml", "continue_on_error: false"),
    "gc-verification": ('name: "gc verification"', 'name: "Run full GC verification"', "run_gc_verification.sh --full --base-sha"),
    "gc-tsan": ('name: "gc thread sanitizer"', 'name: "Run GC ThreadSanitizer tests"', "bash tools/run_gc_tsan_tests.sh"),
}
INVESTMENT_GATE_DECISION_TOKENS = {
    "actor_gate": "The Actor gate is {decision}.",
    "formal_model_gate": "The formal model gate is {decision}.",
    "model_ci_gate": "The Model Consistency CI Gate is therefore {decision}.",
}
RETIRED_SHARED_LOBBY_COMPATIBILITY_SYMBOLS = (
    "GBE_DotaSharedLobbyScalarSnapshot",
    "GBE_GetSharedDotaLobbyScalarSnapshot",
    "GBE_HasSharedDotaLobbyState",
    "GBE_GetSharedDotaLobbyIdOrZero",
    "GBE_GetSharedDotaGenericLobbyIdOrZero",
    "GBE_IsSharedDotaArcadeLobbyActive",
    "GBE_IsDotaArcadeLobbyActive",
    "GBE_GetSharedDotaLobbyStateSnapshot",
    "GBE_ClearSharedDotaLobbyState",
    "GBE_ClearSharedDotaLobbyForRuntimeReset",
    "GBE_shared_dota_lobby_state",
)
POST_LOGIN_REGISTRY_OWNER = "gbe_dota_post_login_dispatcher.cpp"
POST_LOGIN_REGISTRY_CPP = os.path.join(ROOT_DIR, "dll", POST_LOGIN_REGISTRY_OWNER)
RECONNECT_MAPPING_OWNER = "gbe_dota_reconnect_context.cpp"
RECONNECT_SOURCE_FIELDS = (
    "kind",
    "valid",
    "active",
    "generation",
    "lobby_id",
    "lobby_state",
    "game_state",
    "server_id",
    "custom_game_id",
    "owner_connected",
    "launch_phase",
    "owner_steam_id",
    "connect",
)
RECONNECT_CONTEXT_FIELDS = (
    "generation",
    "lobby_id",
    "lobby_state",
    "game_state",
    "server_id",
    "custom_game_id",
    "owner_steam_id",
    "connect",
)
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
CONCURRENCY_OWNERSHIP_TERMS = [
    "Shared lobby store",
    "Recent reconnect context",
    "Serialized connection state",
    "Callback queue",
    "Delayed reconnect callback",
    "Delayed GC message",
    "Deferred lifecycle slot",
    "GBE_GetSharedDotaLobbyStateStore()",
    "GBE_GetRecentDotaReconnectContext()",
    "Steam_Networking_Sockets_Serialized::PostConnectionStateMsg()",
    "Steam_Client::RunCallbacks()",
    "SteamCallResults::runCallResults()",
    "P11.2",
    "P11.3",
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
    start = main_text.find("registry::View Steam_Game_Coordinator::GBE_ProductionDotaHandlerRegistry")
    if start < 0:
        return ["GBE_ProductionDotaHandlerRegistry definition not found"], 0, 0
    end = main_text.find("bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest", start)
    factory_text = main_text[start:end if end >= 0 else len(main_text)]

    table_start = factory_text.find("static const registry::Entry kTable[]")
    table_end = factory_text.find("};", table_start)
    if table_start < 0 or table_end < 0:
        return ["typed post-login registry table not found"], 0, 0
    table_text = factory_text[table_start:table_end]

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
        r"auto\s+(adapt_[A-Za-z0-9_]+)\s*=\s*\+\[\]\(.*?\)\s*->\s*bool\s*\{(.*?)\}\s*;",
        re.DOTALL,
    )
    adapters = dict(adapter_pattern.findall(factory_text[:table_start]))

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
    template_owner = os.path.join(ROOT_DIR, "dll", "gbe_dota_template_replay_templates.cpp")
    if not os.path.exists(template_owner):
        issues.append(
            ("gbe_dota_template_replay_templates.cpp", 0, "missing dedicated template asset TU")
        )
    for path in tu_paths:
        base = os.path.basename(path)
        if base in TEMPLATE_BLOB_OWNER_FILES:
            continue
        if not base.startswith("gbe_dota_") or not base.endswith("_handlers.cpp"):
            continue
        for line_no, line in enumerate(read(path).splitlines(), 1):
            if long_hex_literal.search(line):
                issues.append((base, line_no, "large hex literal"))
    # D.12.1: handler logic TU must not own template static blobs.
    handler_path = os.path.join(ROOT_DIR, "dll", "gbe_dota_template_replay_handlers.cpp")
    if os.path.exists(handler_path):
        handler_text = read(handler_path)
        if "static const uint8 GBE_kDota" in handler_text or "static constexpr const char *GBE_kDota" in handler_text:
            issues.append(
                (
                    "gbe_dota_template_replay_handlers.cpp",
                    0,
                    "template static blobs must live in gbe_dota_template_replay_templates.cpp",
                )
            )
        if "gbe_dota_template_replay_templates.h" not in handler_text:
            issues.append(
                (
                    "gbe_dota_template_replay_handlers.cpp",
                    0,
                    "must include gbe_dota_template_replay_templates.h",
                )
            )
    return issues


def audit_gc_internal_slim_boundary(internal_text=None):
    """D.12.2: gbe_dota_gc_internal.h must not re-export equip/networking/lobby payload/locator."""
    if internal_text is None:
        internal_text = read(INTERNAL_H)
    issues = []
    forbidden_includes = (
        "gbe_dota_payload_lobby_helpers.h",
        "gbe_dota_locator.h",
        "gbe_dota_reconnect_shared.h",
        "gbe_dota_inventory_ports.h",
        "steam_networking",
    )
    # Only flag real #include lines; comments may mention the dedicated headers.
    for line in internal_text.splitlines():
        stripped = line.strip()
        if not stripped.startswith("#include"):
            continue
        for token in forbidden_includes:
            if token in stripped:
                issues.append(f"gbe_dota_gc_internal.h: must not include or re-export '{token}'")
    forbidden_symbols = (
        "GBE_PushDotaPlayerEquippedItemsCacheToGC",
        "GBE_RefreshDotaHostEquippedItemsCache",
        "GBE_PushDotaHeroEquippedItemUpdatesToClientGC",
    )
    for symbol in forbidden_symbols:
        if re.search(r"\b" + re.escape(symbol) + r"\b", internal_text):
            issues.append(f"gbe_dota_gc_internal.h: free equip port '{symbol}' must stay in inventory_ports.h")
    if "Cross-GC equip ports live in gbe_dota_inventory_ports.h" not in internal_text:
        issues.append(
            "gbe_dota_gc_internal.h: must document that free equip ports live in inventory_ports.h"
        )
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


def audit_handler_side_effect_seams(tu_paths=None, source_texts=None, baseline=None):
    """Detect drift in high-risk side effects in ordinary handler files."""
    if source_texts is None:
        source_texts = {
            os.path.basename(path): read(path)
            for path in (tu_paths or GC_TUS)
        }
    if baseline is None:
        baseline = HIGH_RISK_SIDE_EFFECT_HANDLER_BASELINE

    pattern = re.compile(r"\b(" + "|".join(re.escape(api) for api in HIGH_RISK_SIDE_EFFECT_APIS) + r")\s*\(")
    actual = {}
    for source_name, source_text in source_texts.items():
        base = os.path.basename(source_name)
        if not base.startswith("gbe_dota_") or not base.endswith("_handlers.cpp"):
            continue
        text = strip_comments(source_text)
        for match in pattern.finditer(text):
            line_start = text.rfind("\n", 0, match.start()) + 1
            line = text[line_start:match.start()].strip()
            if "::" in line:
                continue
            key = (base, match.group(1))
            actual[key] = actual.get(key, 0) + 1

    issues = []
    for key, actual_count in sorted(actual.items()):
        expected_count = baseline.get(key)
        if expected_count is None:
            issues.append(f"{key[0]}: new high-risk side-effect call to {key[1]} requires an approved seam or explicit baseline entry")
        elif actual_count != expected_count:
            issues.append(f"{key[0]}: {key[1]} count changed from {expected_count} to {actual_count}; route through an approved seam or update the baseline with reason")

    for key, expected_count in sorted(baseline.items()):
        actual_count = actual.get(key, 0)
        if actual_count == 0 and expected_count:
            issues.append(f"{key[0]}: {key[1]} baseline expected {expected_count}, found 0; remove stale baseline entry or confirm the seam migration")

    return issues, sum(actual.values()), len(baseline)


def audit_handler_responsibility_boundaries(source_texts=None, baseline=None):
    """Prevent ordinary handlers from expanding direct state and effect ownership."""
    if source_texts is None:
        source_texts = {
            os.path.basename(path): read(path)
            for path in GC_TUS
        }
    if baseline is None:
        baseline = HANDLER_RESPONSIBILITY_BASELINE

    patterns = {
        "push_incoming_now": re.compile(r"\bpush_incoming_now\s*\("),
        "network sendToAllGameservers": re.compile(r"\bnetwork\s*->\s*sendToAllGameservers\s*\("),
        "GBE_local_lobby assignment": re.compile(r"\bGBE_local_lobby\s*=(?!=)"),
        "shared Store accessor": re.compile(r"\bGBE_GetSharedDotaLobbyStateStore\s*\("),
    }
    actual = {}
    for source_name, source_text in source_texts.items():
        base = os.path.basename(source_name)
        if not base.startswith("gbe_dota_") or not base.endswith("_handlers.cpp"):
            continue
        uncommented = strip_comments(source_text)
        for responsibility, pattern in patterns.items():
            count = len(pattern.findall(uncommented))
            if count:
                actual[(base, responsibility)] = count

    issues = []
    for key, count in sorted(actual.items()):
        allowed = baseline.get(key, 0)
        if count > allowed:
            issues.append(
                f"{key[0]}: direct {key[1]} count {count} exceeds accepted handler boundary {allowed}; route new work through a coordinator or executor"
            )
    return issues, actual


def audit_state_effect_ownership(source_texts=None):
    """Keep pure planning, shared state, lifecycle effects, and reconnect network calls on canonical owners."""
    if source_texts is None:
        source_texts = {
            base: read(os.path.join(ROOT_DIR, "dll", base))
            for base in PURE_DOTA_PLANNER_FILES
        }
        source_texts[LIFECYCLE_EXECUTOR_OWNER] = read(os.path.join(ROOT_DIR, "dll", LIFECYCLE_EXECUTOR_OWNER))
        source_texts["gbe_dota_reconnect_network_adapter.cpp"] = read(
            os.path.join(ROOT_DIR, "dll", "gbe_dota_reconnect_network_adapter.cpp")
        )
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "gbe_dota_*.cpp")):
            source_texts.setdefault(os.path.basename(path), read(path))

    issues = []
    for base in PURE_DOTA_PLANNER_FILES:
        source = strip_comments(source_texts.get(base, ""))
        for token in PURE_DOTA_PLANNER_FORBIDDEN_TOKENS:
            if token in source:
                issues.append(f"{base}: pure planner directly uses side-effect or shared-state token {token}")

    executor = strip_comments(source_texts.get(LIFECYCLE_EXECUTOR_OWNER, ""))
    for api in LIFECYCLE_SIDE_EFFECT_APIS:
        if not re.search(r"\b" + re.escape(api) + r"\s*\(", executor):
            issues.append(f"{LIFECYCLE_EXECUTOR_OWNER}: lifecycle executor no longer owns {api}")

    for source_name, source_text in source_texts.items():
        base = os.path.basename(source_name)
        if not base.startswith("gbe_dota_") or not base.endswith(".cpp"):
            continue
        if base == "gbe_dota_reconnect_network_adapter.cpp":
            continue
        if re.search(r"\bConnectByIPAddress\s*\(", strip_comments(source_text)):
            issues.append(f"{base}: Dota reconnect network call bypasses gbe_dota_reconnect_network_adapter.cpp")

    adapter = strip_comments(source_texts.get("gbe_dota_reconnect_network_adapter.cpp", ""))
    if not re.search(r"\bConnectByIPAddress\s*\(", adapter):
        issues.append("gbe_dota_reconnect_network_adapter.cpp: canonical direct-connect call is missing")
    return issues


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


def audit_lifecycle_transition_gates(source_texts=None):
    """Keep migrated lifecycle paths behind typed transition effects."""
    if source_texts is None:
        source_texts = {
            base: read(os.path.join(ROOT_DIR, "dll", base))
            for base in LIFECYCLE_TRANSITION_GATES
        }

    issues = []
    for base, required_tokens in LIFECYCLE_TRANSITION_GATES.items():
        source = strip_comments(source_texts.get(base, ""))
        for token in required_tokens:
            if token not in source:
                issues.append(f"{base}: lifecycle path is missing transition gate token {token}")
    return issues


def audit_core_state_machine_boundaries(
    lifecycle_header_text=None,
    generation_header_text=None,
    production_sources=None,
):
    """Keep lifecycle, generation, and reconnect transitions on pure canonical owners."""
    if lifecycle_header_text is None:
        lifecycle_header_text = read(os.path.join(ROOT_DIR, "dll", "gbe_dota_lifecycle_state_machine.h"))
    if generation_header_text is None:
        generation_header_text = read(os.path.join(ROOT_DIR, "dll", "gbe_dota_lobby_generation.h"))
    if production_sources is None:
        production_sources = {
            os.path.basename(path): read(path)
            for path in glob.glob(os.path.join(ROOT_DIR, "dll", "*.cpp"))
        }

    issues = []
    for token in CORE_STATE_MACHINE_HEADER_TOKENS:
        if token not in lifecycle_header_text:
            issues.append(f"gbe_dota_lifecycle_state_machine.h: missing pure core transition {token}")
    if "constexpr AdvanceResult advance(Boundary boundary)" not in generation_header_text:
        issues.append("gbe_dota_lobby_generation.h: generation advance must remain a constexpr value transition")

    generation_advance_owners = []
    generation_sync_owners = []
    reconnect_state_owners = []
    for filename, source_text in production_sources.items():
        source = strip_comments(source_text)
        if ".advance(" in source and "GBE_dota_lobby_generation_counter" in source:
            generation_advance_owners.append(filename)
        sync_count = len(re.findall(r"GBE_dota_lobby_generation_counter\s*=\s*gbe::dota_lobby_generation::Counter\s*\(", source))
        generation_sync_owners.extend([filename] * sync_count)
        if any(token in source for token in (
            "connection_state.begin_generation(",
            "connection_state.should_connect_direct(",
            "connection_state.record_direct_connect(",
            "connection_state.record_engine_callback(",
        )):
            reconnect_state_owners.append(filename)

    if generation_advance_owners != [GENERATION_COUNTER_OWNER]:
        issues.append(
            f"generation counter advance owners changed: expected [{GENERATION_COUNTER_OWNER}], got {sorted(generation_advance_owners)}"
        )
    if generation_sync_owners != [GENERATION_COUNTER_SYNC_OWNER] * GENERATION_COUNTER_SYNC_BASELINE:
        issues.append(
            f"generation counter synchronization changed: expected {GENERATION_COUNTER_SYNC_BASELINE} assignments in {GENERATION_COUNTER_SYNC_OWNER}, got {sorted(generation_sync_owners)}"
        )
    if sorted(set(reconnect_state_owners)) != [RECONNECT_TRANSITION_OWNER]:
        issues.append(
            f"reconnect generation/dedup transition owners changed: expected [{RECONNECT_TRANSITION_OWNER}], got {sorted(set(reconnect_state_owners))}"
        )
    reconnect_owner = strip_comments(production_sources.get(RECONNECT_TRANSITION_OWNER, ""))
    for token in ("GBE_PrepareDotaReconnectPostConnectionState(", "GBE_ExecuteDotaReconnectPostEffects("):
        if token not in reconnect_owner:
            issues.append(f"{RECONNECT_TRANSITION_OWNER}: missing reconnect planner/executor boundary {token}")
    return issues


def audit_async_generation_safety(source_texts=None, handler_test_text=None):
    """Keep every delayed GC boundary generation-scoped through final execution."""
    if source_texts is None:
        source_texts = {
            filename: read(os.path.join(ROOT_DIR, "dll", filename))
            for filename in ASYNC_GENERATION_GATES
        }
    if handler_test_text is None:
        handler_test_text = read(os.path.join(ROOT_DIR, "tools", "gbe_dota_handler_test", "smoke_test.cpp"))

    issues = []
    for filename, required_tokens in ASYNC_GENERATION_GATES.items():
        source = strip_comments(source_texts.get(filename, ""))
        for token in required_tokens:
            if token not in source:
                issues.append(f"{filename}: async generation boundary is missing {token}")
    for test_name in ASYNC_GENERATION_REGRESSION_TESTS:
        if handler_test_text.count(test_name) < 2:
            issues.append(f"gbe_dota_handler_test/smoke_test.cpp: missing executed async generation regression {test_name}")
    return issues


def audit_test_credibility(test_sources=None, runner_text=None):
    """Keep high-risk logic, executor, fake integration, property, and model tests executed."""
    if test_sources is None:
        test_sources = {
            path: read(os.path.join(ROOT_DIR, path))
            for path in TEST_CREDIBILITY_GATES
        }
    if runner_text is None:
        runner_text = read(os.path.join(ROOT_DIR, "tools", "run_gc_offline_tests.sh"))

    issues = []
    for path, test_names in TEST_CREDIBILITY_GATES.items():
        source = strip_comments(test_sources.get(path, ""))
        for test_name in test_names:
            if source.count(test_name) < 2:
                issues.append(f"{path}: high-risk test is not defined and executed: {test_name}")
    for target in TEST_CREDIBILITY_RUNNER_TARGETS:
        if runner_text.count(target) < 2:
            issues.append(f"tools/run_gc_offline_tests.sh: high-risk test target is not built and run: {target}")
    return issues


def audit_architecture_investment_inputs(input_text=None):
    """Keep optional architecture decisions tied to complete repeatable inputs."""
    if input_text is None:
        input_text = read(ARCHITECTURE_INVESTMENT_INPUTS_JSON)
    try:
        inputs = json.loads(input_text)
    except (TypeError, json.JSONDecodeError) as exc:
        return [f"architecture-investment-inputs.json: invalid JSON: {exc}"]

    issues = []
    required_triggers = {
        "major_concurrency_expansion",
        "major_lifecycle_state_machine_expansion",
    }
    if inputs.get("schema_version") != 1:
        issues.append("architecture-investment-inputs.json: schema_version must be 1")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", str(inputs.get("decision_date", ""))):
        issues.append("architecture-investment-inputs.json: decision_date must use YYYY-MM-DD")
    if not required_triggers.issubset(set(inputs.get("recheck_triggers", []))):
        issues.append("architecture-investment-inputs.json: both major expansion recheck triggers are required")

    concurrency = inputs.get("concurrency", {})
    for field in (
        "tsan_race_reports",
        "lock_order_depth",
        "out_of_order_mutation_defects_in_phase",
        "unowned_cross_thread_business_writers",
    ):
        if not isinstance(concurrency.get(field), int) or concurrency[field] < 0:
            issues.append(f"architecture-investment-inputs.json: concurrency.{field} must be a nonnegative integer")
    if concurrency.get("actor_gate") not in {"open", "closed"}:
        issues.append("architecture-investment-inputs.json: concurrency.actor_gate must be open or closed")

    state_machine = inputs.get("state_machine", {})
    for field in (
        "state_count",
        "event_count",
        "state_event_pairs",
        "length_five_sequences_checked",
        "differential_steps_checked",
        "escaped_ordering_defects_in_phase",
        "active_transition_maintainers_in_release",
    ):
        if not isinstance(state_machine.get(field), int) or state_machine[field] < 0:
            issues.append(f"architecture-investment-inputs.json: state_machine.{field} must be a nonnegative integer")
    if all(isinstance(state_machine.get(field), int) for field in ("state_count", "event_count", "state_event_pairs")):
        expected_pairs = state_machine["state_count"] * state_machine["event_count"]
        if state_machine["state_event_pairs"] != expected_pairs:
            issues.append("architecture-investment-inputs.json: state_event_pairs must equal state_count * event_count")
    if not isinstance(state_machine.get("critical_irreversible_failure_scope"), bool):
        issues.append("architecture-investment-inputs.json: critical_irreversible_failure_scope must be boolean")
    if state_machine.get("formal_model_gate") not in {"open", "closed"}:
        issues.append("architecture-investment-inputs.json: state_machine.formal_model_gate must be open or closed")

    model = inputs.get("model_consistency", {})
    for field in ("maintained_formal_model", "named_owner_and_reviewer", "versioned_vector_schema", "pinned_checker"):
        if not isinstance(model.get(field), bool):
            issues.append(f"architecture-investment-inputs.json: model_consistency.{field} must be boolean")
    if model.get("model_ci_gate") not in {"open", "closed"}:
        issues.append("architecture-investment-inputs.json: model_consistency.model_ci_gate must be open or closed")

    ci_seconds = inputs.get("ci_seconds", {})
    for field in ("local_fast_offline", "local_clang_tsan", "model_checker_limit"):
        value = ci_seconds.get(field)
        if not isinstance(value, (int, float)) or isinstance(value, bool) or value <= 0:
            issues.append(f"architecture-investment-inputs.json: ci_seconds.{field} must be positive")
    return issues


def audit_architecture_investment_boundaries(input_text=None, gates_text=None, tasklist_text=None):
    """Derive optional architecture decisions from evidence and keep records aligned."""
    if input_text is None:
        input_text = read(ARCHITECTURE_INVESTMENT_INPUTS_JSON)
    if gates_text is None:
        gates_text = read(os.path.join(ROOT_DIR, "docs", "gc", "architecture-investment-gates.md"))
    if tasklist_text is None:
        tasklist_text = read(os.path.join(ROOT_DIR, ".monkeycode", "specs", "gc-refactor-next-phase", "tasklist.md"))
    try:
        inputs = json.loads(input_text)
    except (TypeError, json.JSONDecodeError) as exc:
        return [f"architecture-investment-inputs.json: invalid JSON for gate derivation: {exc}"]

    concurrency = inputs.get("concurrency", {})
    state_machine = inputs.get("state_machine", {})
    model = inputs.get("model_consistency", {})
    actor_open = (
        concurrency.get("tsan_race_reports", 0) > 0
        or concurrency.get("lock_order_depth", 0) > 2
        or concurrency.get("out_of_order_mutation_defects_in_phase", 0) >= 2
        or concurrency.get("unowned_cross_thread_business_writers", 0) > 0
    )
    formal_conditions = sum((
        state_machine.get("state_count", 0) > 16
        or state_machine.get("event_count", 0) > 24
        or state_machine.get("state_event_pairs", 0) > 384,
        state_machine.get("escaped_ordering_defects_in_phase", 0) >= 2,
        state_machine.get("active_transition_maintainers_in_release", 0) >= 3,
    ))
    formal_open = bool(state_machine.get("critical_irreversible_failure_scope", False)) or formal_conditions >= 2
    model_open = formal_open and all(
        model.get(field) is True
        for field in ("maintained_formal_model", "named_owner_and_reviewer", "versioned_vector_schema", "pinned_checker")
    )
    expected = {
        "actor_gate": "open" if actor_open else "closed",
        "formal_model_gate": "open" if formal_open else "closed",
        "model_ci_gate": "open" if model_open else "closed",
    }
    recorded = {
        "actor_gate": concurrency.get("actor_gate"),
        "formal_model_gate": state_machine.get("formal_model_gate"),
        "model_ci_gate": model.get("model_ci_gate"),
    }

    issues = []
    for gate, decision in expected.items():
        if recorded[gate] != decision:
            issues.append(f"architecture-investment-inputs.json: {gate} must be {decision} for the recorded evidence")
        document_token = INVESTMENT_GATE_DECISION_TOKENS[gate].format(decision=decision)
        if document_token not in gates_text:
            issues.append(f"architecture-investment-gates.md: missing derived decision '{document_token}'")
    task_tokens = (
        f"Actor 门禁{'关闭' if expected['actor_gate'] == 'closed' else '开启'}",
        f"形式化模型门禁{'关闭' if expected['formal_model_gate'] == 'closed' else '开启'}",
        f"P17 门禁{'关闭' if expected['model_ci_gate'] == 'closed' else '开启'}",
    )
    for token in task_tokens:
        if token not in tasklist_text:
            issues.append(f"tasklist.md: missing derived investment decision {token}")
    return issues


def audit_retired_lifecycle_transition_layers(source_texts=None):
    """Prevent retired lifecycle entrypoints and manual dispatch fallbacks from returning."""
    if source_texts is None:
        source_texts = {
            "steam_game_coordinator.h": read(os.path.join(ROOT_DIR, "dll", "dll", "steam_game_coordinator.h")),
            "gbe_dota_match_handlers.cpp": read(os.path.join(ROOT_DIR, "dll", "gbe_dota_match_handlers.cpp")),
            "gbe_dota_post_login_handlers.cpp": read(os.path.join(ROOT_DIR, "dll", "gbe_dota_post_login_handlers.cpp")),
        }

    issues = []
    for source_name, source_text in source_texts.items():
        uncommented = strip_comments(source_text)
        for symbol in RETIRED_LIFECYCLE_HANDLER_SYMBOLS:
            if re.search(r"\b" + re.escape(symbol) + r"\b", uncommented):
                issues.append(f"{source_name}: retired lifecycle handler {symbol} returned")

    post_login_text = strip_comments(source_texts.get("gbe_dota_post_login_handlers.cpp", ""))
    for emsg in RETIRED_LIFECYCLE_FALLBACK_EMSGS:
        if re.search(r"\b" + re.escape(emsg) + r"\b", post_login_text):
            issues.append(f"gbe_dota_post_login_handlers.cpp: lifecycle emsg {emsg} bypasses the typed registry")
    return issues


def audit_retired_reconnect_transition_layers(source_texts=None):
    """Prevent retired reconnect mappings and lobby-id state APIs from returning."""
    if source_texts is None:
        source_texts = {}
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "**", "*.h"), recursive=True):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "*.cpp")):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)

    issues = []
    for source_name, source_text in source_texts.items():
        uncommented = strip_comments(source_text)
        for symbol in RETIRED_RECONNECT_TRANSITION_SYMBOLS:
            if re.search(r"\b" + re.escape(symbol) + r"\b", uncommented):
                issues.append(f"{source_name}: retired reconnect transition symbol {symbol} returned")

    serialized_header = strip_comments(source_texts.get(
        "dll/dll/gbe_dota_serialized_connection_state.h",
        source_texts.get("gbe_dota_serialized_connection_state.h", ""),
    ))
    state_match = re.search(
        r"struct\s+GBE_DotaSerializedConnectionState\s*\{(?P<body>.*?)\n\s*\};",
        serialized_header,
        re.S,
    )
    if not state_match:
        issues.append("gbe_dota_serialized_connection_state.h: serialized reconnect state declaration is missing")
        return issues

    state_body = state_match.group("body")
    if re.search(r"\blobby_id\b", state_body):
        issues.append("gbe_dota_serialized_connection_state.h: serialized reconnect state restored lobby_id compatibility state")
    if re.search(r"\bbegin_lobby\s*\(", state_body):
        issues.append("gbe_dota_serialized_connection_state.h: serialized reconnect state restored begin_lobby compatibility API")

    begin_generation = re.search(r"\bbegin_generation\s*\((?P<parameters>[^)]*)\)", state_body)
    if not begin_generation:
        issues.append("gbe_dota_serialized_connection_state.h: generation-scoped begin_generation API is missing")
    elif "=" in begin_generation.group("parameters"):
        issues.append("gbe_dota_serialized_connection_state.h: begin_generation restored a default generation")
    return issues


def audit_retired_shared_lobby_compatibility_layers(source_texts=None):
    """Prevent shared lobby projections and mutable backing state from returning."""
    if source_texts is None:
        source_texts = {}
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "**", "*.h"), recursive=True):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "*.cpp")):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)

    issues = []
    for source_name, source_text in source_texts.items():
        uncommented = strip_comments(source_text)
        for symbol in RETIRED_SHARED_LOBBY_COMPATIBILITY_SYMBOLS:
            if re.search(r"\b" + re.escape(symbol) + r"\b", uncommented):
                issues.append(f"{source_name}: retired shared lobby compatibility symbol {symbol} returned")
    return issues


def audit_architecture_boundaries(source_texts=None):
    """Keep handler effects, post-login dispatch, reconnect mapping, and shared state on canonical owners."""
    injected = source_texts is not None
    if source_texts is None:
        source_texts = {}
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "**", "*.h"), recursive=True):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "*.cpp")):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)

    issues = []
    side_effect_issues, _, _ = audit_handler_side_effect_seams(
        source_texts=source_texts,
        baseline={} if injected else HIGH_RISK_SIDE_EFFECT_HANDLER_BASELINE,
    )
    issues.extend(side_effect_issues)

    registry_declaration = re.compile(r"\bregistry::Entry\s+[A-Za-z_][A-Za-z0-9_]*\s*\[")
    post_login_switch = re.compile(r"\bswitch\s*\(\s*(?:request_emsg|inner_emsg)\s*\)")
    for source_name, source_text in source_texts.items():
        base = os.path.basename(source_name)
        uncommented = strip_comments(source_text)
        registry_count = len(registry_declaration.findall(uncommented))
        if base == POST_LOGIN_REGISTRY_OWNER:
            if registry_count > 1:
                issues.append(f"{base}: parallel typed post-login registry returned beside the canonical kTable")
            dispatch_start = uncommented.find("bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest")
            dispatch_end = uncommented.find("bool Steam_Game_Coordinator::gc_enabled", dispatch_start)
            dispatch_text = uncommented[dispatch_start:dispatch_end if dispatch_end >= 0 else len(uncommented)]
            if "static const registry::Entry" in dispatch_text:
                issues.append(f"{base}: post-login dispatcher owns a registry table instead of consuming the injected view")
            if "handler_registry.entries" not in dispatch_text or "handler_registry.size" not in dispatch_text:
                issues.append(f"{base}: post-login dispatcher does not consume the injected registry view")
        elif registry_count:
            issues.append(f"{base}: parallel typed post-login registry returned outside {POST_LOGIN_REGISTRY_OWNER}")

        if base in {POST_LOGIN_REGISTRY_OWNER, "gbe_dota_post_login_handlers.cpp"} and post_login_switch.search(uncommented):
            issues.append(f"{base}: post-login message switch bypasses the typed registry")

        if (
            base.startswith("gbe_dota_")
            and (base.endswith("_handlers.cpp") or base.endswith("_coordinator.cpp"))
            and "GBE_GetSharedDotaLobbyStateStore()" in uncommented
        ):
            issues.append(f"{base}: coordinator business path bypasses the injected shared lobby Store")

    reconnect_types = (
        (r"(?:GBE_DotaReconnectSource|(?:gbe::)?dota_reconnect::Source)", "source", RECONNECT_SOURCE_FIELDS),
        (r"GBE_DotaReconnectContext", "context", RECONNECT_CONTEXT_FIELDS),
    )
    for source_name, source_text in source_texts.items():
        base = os.path.basename(source_name)
        if base == RECONNECT_MAPPING_OWNER or not base.endswith(".cpp"):
            continue
        uncommented = strip_comments(source_text)
        for type_pattern, mapping_name, fields in reconnect_types:
            declarations = re.finditer(
                r"\b" + type_pattern + r"\s*(?:[&*]\s*)?([A-Za-z_][A-Za-z0-9_]*)",
                uncommented,
            )
            for declaration in declarations:
                variable = declaration.group(1)
                assignment = re.compile(
                    r"\b" + re.escape(variable) + r"\s*(?:\.|->)\s*(" + "|".join(fields) + r")\s*=(?!=)",
                )
                for match in assignment.finditer(uncommented, declaration.end()):
                    issues.append(
                        f"{base}: reconnect {mapping_name} field {match.group(1)} is mapped outside {RECONNECT_MAPPING_OWNER}"
                    )

    issues.extend(audit_retired_shared_lobby_compatibility_layers(source_texts))
    return issues


def extract_yaml_job(workflow_text, job_name):
    match = re.search(r"^(?P<indent>[ \t]*)" + re.escape(job_name) + r":[ \t]*$", workflow_text, re.MULTILINE)
    if not match:
        return ""
    indent = match.group("indent")
    next_job = re.search(r"^" + re.escape(indent) + r"[A-Za-z0-9_-]+:[ \t]*$", workflow_text[match.end():], re.MULTILINE)
    end = match.end() + next_job.start() if next_job else len(workflow_text)
    return workflow_text[match.start():end]


def audit_layered_ci_gates(workflow_text=None, verification_text=None, offline_text=None, tsan_text=None):
    """Keep PR checks split into fast, production, and sanitizer blocking layers."""
    if workflow_text is None:
        workflow_text = read(PR_WORKFLOW_YML)
    if verification_text is None:
        verification_text = read(RUN_GC_OFFLINE_TESTS_SH.replace("run_gc_offline_tests.sh", "run_gc_verification.sh"))
    if offline_text is None:
        offline_text = read(RUN_GC_OFFLINE_TESTS_SH)
    if tsan_text is None:
        tsan_text = read(os.path.join(ROOT_DIR, "tools", "run_gc_tsan_tests.sh"))

    issues = []
    full_job = extract_yaml_job(workflow_text, "gc-verification")
    if (
        "run_gc_verification.sh --full --base-sha" not in full_job
        or "github.event.pull_request.base.sha" not in full_job
        or "continue-on-error: true" in full_job
    ):
        issues.append("emu-pull-request.yml: full GC layer must run run_gc_verification.sh --full with the PR base SHA")

    production_jobs = (
        ("emu-win-release", "emu-build-all-win.yml", "Windows"),
        ("emu-linux-release", "emu-build-all-linux.yml", "Linux"),
    )
    for job_name, reusable_workflow, platform in production_jobs:
        job = extract_yaml_job(workflow_text, job_name)
        required_patterns = (
            re.escape(reusable_workflow),
            r"matrix_prj:\s*['\"]?\[[^\]]*api_experimental[^\]]*\]",
            r"matrix_arch:\s*['\"]?\[[^\]]*x64[^\]]*\]",
            r"matrix_cfg:\s*['\"]?\[[^\]]*release[^\]]*\]",
            r"continue_on_error:\s*false\b",
        )
        if any(not re.search(pattern, job) for pattern in required_patterns):
            issues.append(
                f"emu-pull-request.yml: {platform} production layer must build api_experimental x64 release with failures blocking"
            )

    tsan_job = extract_yaml_job(workflow_text, "gc-tsan")
    if (
        "CXX: clang++" not in tsan_job
        or "bash tools/run_gc_tsan_tests.sh" not in tsan_job
        or "continue-on-error: true" in tsan_job
    ):
        issues.append("emu-pull-request.yml: TSAN layer must use Clang and run the dedicated sanitizer script")

    verification_requirements = (
        ("tools/run_gc_offline_tests.sh", "offline unit/property/replay execution"),
        ("python3 tools/_audit_gc_refactor.py", "architecture audit execution"),
        ("git diff --check", "diff check execution"),
    )
    for required, description in verification_requirements:
        if required not in verification_text:
            issues.append(f"run_gc_verification.sh: full layer is missing {description}")

    offline_requirements = (
        "python3 tools/test_audit_gc_refactor.py",
        "gbe_dota_reconnect_network_test",
        "gbe_dota_lobby_state_store_test",
        "gbe_dota_concurrency_stress_test",
        "gbe_dota_handler_registry_test",
        "gc_replay_test",
    )
    for required in offline_requirements:
        if required not in offline_text:
            issues.append(f"run_gc_offline_tests.sh: fast layer is missing required target {required}")

    for required in (
        "-fsanitize=thread",
        "halt_on_error=1:exitcode=66",
        "gbe_dota_reconnect_network_test",
        "gbe_dota_concurrency_stress_test",
    ):
        if required not in tsan_text:
            issues.append(f"run_gc_tsan_tests.sh: sanitizer layer is missing required boundary {required}")
    return issues


def audit_ci_failure_localization(workflow_text=None, verification_text=None, tsan_text=None):
    """Keep blocking GC layers independently named and fail-fast for diagnosis."""
    if workflow_text is None:
        workflow_text = read(PR_WORKFLOW_YML)
    if verification_text is None:
        verification_text = read(os.path.join(ROOT_DIR, "tools", "run_gc_verification.sh"))
    if tsan_text is None:
        tsan_text = read(os.path.join(ROOT_DIR, "tools", "run_gc_tsan_tests.sh"))

    issues = []
    for job_name, required_tokens in CI_LOCALIZATION_GATES.items():
        job = extract_yaml_job(workflow_text, job_name)
        if not job:
            issues.append(f"emu-pull-request.yml: missing independently reportable CI job {job_name}")
            continue
        for token in required_tokens:
            if token not in job:
                issues.append(f"emu-pull-request.yml: {job_name} is missing failure localization token {token}")
    for script_name, script_text in (
        ("run_gc_verification.sh", verification_text),
        ("run_gc_tsan_tests.sh", tsan_text),
    ):
        if "set -euo pipefail" not in script_text:
            issues.append(f"{script_name}: CI gate must fail fast with set -euo pipefail")
    if "GC verification passed" not in verification_text:
        issues.append("run_gc_verification.sh: fast gate is missing an explicit success marker")
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


def audit_store_write_discipline(source_texts=None):
    """Ban bare Store::publish/update from production dll; tests may still bootstrap."""
    if source_texts is None:
        source_texts = {}
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "**", "*.h"), recursive=True):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)
        for path in glob.glob(os.path.join(ROOT_DIR, "dll", "*.cpp")):
            source_texts[os.path.relpath(path, ROOT_DIR)] = read(path)

    # Match store-style receivers only (SharedLobbyStore / GetShared... / lobby_store / bare store).
    bare_write = re.compile(
        r"(?P<recv>"
        r"GBE_SharedLobbyStore\s*\(\s*\)|"
        r"GBE_GetSharedDotaLobbyStateStore\s*\(\s*\)|"
        r"[A-Za-z_][A-Za-z0-9_]*(?:\s*\(\s*\))?"
        r")"
        r"\s*(?:\.|->)\s*"
        r"(?P<method>publish|update)\s*\("
    )
    issues = []
    store_impl = "gbe_dota_lobby_state_store.cpp"
    store_header = "gbe_dota_lobby_state_store.h"

    for source_name, source_text in source_texts.items():
        base = os.path.basename(source_name)
        if base in {store_impl, store_header}:
            continue
        uncommented = strip_comments(source_text)
        for match in bare_write.finditer(uncommented):
            method = match.group("method")
            # Skip gated APIs that embed publish/update in longer names.
            window_start = max(0, match.start() - 64)
            window = uncommented[window_start:match.end()]
            if method == "publish" and "publish_if_generation" in window:
                continue
            if method == "update" and "compare_update" in window:
                continue
            # Require receiver to look like a Store handle (avoid unrelated .update).
            recv = match.group("recv")
            if not re.search(
                r"(?:SharedLobbyStore|GetSharedDotaLobbyStateStore|lobby_store|\bstore\b)",
                recv,
            ):
                continue
            line_no = uncommented.count("\n", 0, match.start()) + 1
            issues.append(
                f"{base}:{line_no}: bare Store::{method}() in production; use publish_if_generation_current_or_newer / compare_update / compare_clear"
            )
    return issues


def audit_composition_root_lifecycle(
    steam_client_text=None,
    steam_client_header_text=None,
    coordinator_text=None,
    welcome_text=None,
    inventory_text=None,
):
    """Keep production GC owners inside their declared dependency lifetime."""
    issues = []
    if steam_client_text is None:
        steam_client_text = read(os.path.join(ROOT_DIR, "dll", "steam_client.cpp"))
    if steam_client_header_text is None:
        steam_client_header_text = read(os.path.join(ROOT_DIR, "dll", "dll", "steam_client.h"))
    if coordinator_text is None:
        coordinator_text = read(os.path.join(ROOT_DIR, "dll", "steam_game_coordinator.cpp"))
    if welcome_text is None:
        welcome_text = read(os.path.join(ROOT_DIR, "dll", "gbe_dota_welcome_coordinator.cpp"))
    if inventory_text is None:
        inventory_text = read(os.path.join(ROOT_DIR, "dll", "gbe_dota_inventory_handlers.cpp"))

    constructor_start = steam_client_text.find("Steam_Client::Steam_Client()")
    destructor_start = steam_client_text.find("Steam_Client::~Steam_Client()")
    constructor_text = steam_client_text[constructor_start:destructor_start]
    destructor_text = steam_client_text[destructor_start:]
    roles = (
        (
            "client",
            "steam_networking_sockets = new Steam_Networking_Sockets(",
            "dota_reconnect_adapter_client = new GBE_DotaReconnectNetworkAdapter(",
            "steam_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized(",
            "dota_lifecycle_executor_client = new gbe::dota_lifecycle::CoordinatorExecutor(",
            "steam_game_coordinator = new Steam_Game_Coordinator(",
            "DEL_INST(steam_game_coordinator);",
            "DEL_INST(dota_lifecycle_executor_client);",
            "DEL_INST(steam_networking_sockets_serialized);",
            "DEL_INST(dota_reconnect_adapter_client);",
            "DEL_INST(steam_networking_sockets);",
        ),
        (
            "gameserver",
            "steam_gameserver_networking_sockets = new Steam_Networking_Sockets(",
            "dota_reconnect_adapter_server = new GBE_DotaReconnectNetworkAdapter(",
            "steam_gameserver_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized(",
            "dota_lifecycle_executor_server = new gbe::dota_lifecycle::CoordinatorExecutor(",
            "steam_gameserver_game_coordinator = new Steam_Game_Coordinator(",
            "DEL_INST(steam_gameserver_game_coordinator);",
            "DEL_INST(dota_lifecycle_executor_server);",
            "DEL_INST(steam_gameserver_networking_sockets_serialized);",
            "DEL_INST(dota_reconnect_adapter_server);",
            "DEL_INST(steam_gameserver_networking_sockets);",
        ),
    )
    for role, direct_new, adapter_new, serialized_new, executor_new, coordinator_new, coordinator_del, executor_del, serialized_del, adapter_del, direct_del in roles:
        construction = tuple(constructor_text.find(token) for token in (direct_new, adapter_new, serialized_new, executor_new, coordinator_new))
        if min(construction) < 0 or not construction[0] < construction[1] < construction[2] < construction[3] < construction[4]:
            issues.append(
                f"steam_client.cpp: {role} GC construction must order direct sockets, reconnect adapter, serialized services, lifecycle executor, then coordinator"
            )
        destruction = tuple(destructor_text.find(token) for token in (coordinator_del, executor_del, serialized_del, adapter_del, direct_del))
        if min(destruction) < 0 or not destruction[0] < destruction[1] < destruction[2] < destruction[3] < destruction[4]:
            issues.append(
                f"steam_client.cpp: {role} GC destruction must order coordinator, lifecycle executor, serialized services, reconnect adapter, then direct sockets"
            )

    serialized_header = read(os.path.join(ROOT_DIR, "dll", "dll", "steam_networking_socketsserialized.h"))
    if "production_reconnect_adapter" in strip_comments(serialized_header):
        issues.append("steam_networking_socketsserialized.h: serialized service owns a hidden production reconnect adapter")

    if "GBE_SharedDotaLobbyState dota_lobby_state" not in steam_client_header_text or "dota_lobby_state::Store dota_lobby_store" not in steam_client_header_text:
        issues.append("steam_client.h: Steam_Client must own the shared Dota lobby backing state and Store")
    if "gbe::dota::RuntimeState dota_runtime_state" not in steam_client_header_text:
        issues.append("steam_client.h: Steam_Client must own the Dota runtime state")
    store_accessor_start = coordinator_text.find("gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore()")
    store_accessor_end = coordinator_text.find("const GBE_DotaLootListData &GBE_GetDotaVpkLootData", store_accessor_start)
    store_accessor = coordinator_text[store_accessor_start:store_accessor_end if store_accessor_end >= 0 else len(coordinator_text)]
    if "static GBE_SharedDotaLobbyState state" in store_accessor or "static gbe::dota_lobby_state::Store store" in store_accessor:
        issues.append("steam_game_coordinator.cpp: shared lobby accessor owns hidden singleton backing state")
    guard_member = "std::unique_ptr<gbe::dota::LocatorBindingGuard> dota_locator_binding"
    guard_create = "dota_locator_binding = std::make_unique<gbe::dota::LocatorBindingGuard>(dota_lobby_store, dota_runtime_state);"
    if guard_member not in steam_client_header_text:
        issues.append("steam_client.h: Steam_Client must own the Dota locator binding guard")
    guard_position = constructor_text.find(guard_create)
    first_coordinator_position = constructor_text.find("steam_game_coordinator = new Steam_Game_Coordinator(")
    if guard_position < 0 or first_coordinator_position < 0 or guard_position > first_coordinator_position:
        issues.append("steam_client.cpp: Steam_Client must bind Dota locators before coordinator construction")
    if "dota_locator_binding.reset();" not in destructor_text:
        issues.append("steam_client.cpp: Steam_Client must release the Dota locator binding guard during destruction")
    retired_runtime_state = (
        ("steam_game_coordinator.cpp", coordinator_text, "GBE_recent_dota_reconnect_context_valid"),
        ("steam_game_coordinator.cpp", coordinator_text, "GBE_recent_dota_reconnect_context"),
        ("steam_game_coordinator.cpp", coordinator_text, "GBE_dota_reconnect_eligible"),
        ("steam_game_coordinator.cpp", coordinator_text, "GBE_last_dota_server_hello_context"),
        ("steam_game_coordinator.cpp", coordinator_text, "GBE_vpk_loot_data"),
        ("gbe_dota_welcome_coordinator.cpp", welcome_text, "vpk_items_loaded"),
        ("gbe_dota_welcome_coordinator.cpp", welcome_text, "vpk_item_defs"),
        ("gbe_dota_welcome_coordinator.cpp", welcome_text, "vpk_style_unlock"),
        ("gbe_dota_welcome_coordinator.cpp", welcome_text, "vpk_items_disabled"),
        ("gbe_dota_inventory_handlers.cpp", inventory_text, "equip_cache_version"),
    )
    for filename, source_text, symbol in retired_runtime_state:
        if re.search(r"^(?:static\s+)?[^\n;=.]*\b" + re.escape(symbol) + r"\b\s*(?:\{|=|;)", source_text, re.MULTILINE):
            issues.append(f"{filename}: retired file-level Dota runtime state {symbol} returned")
    return issues


def audit_dependency_object_lifecycle(
    composition_header_text=None,
    composition_source_text=None,
    composition_test_text=None,
    coordinator_header_text=None,
    steam_client_header_text=None,
):
    """Keep GC state application-owned, dependencies explicit, and roots isolated."""
    if composition_header_text is None:
        composition_header_text = read(os.path.join(ROOT_DIR, "dll", "gbe_dota_composition_root.h"))
    if composition_source_text is None:
        composition_source_text = read(os.path.join(ROOT_DIR, "dll", "gbe_dota_composition_root.cpp"))
    if composition_test_text is None:
        composition_test_text = read(os.path.join(ROOT_DIR, "tools", "gbe_dota_composition_root_test", "gbe_dota_composition_root_test.cpp"))
    if coordinator_header_text is None:
        coordinator_header_text = read(os.path.join(ROOT_DIR, "dll", "dll", "steam_game_coordinator.h"))
    if steam_client_header_text is None:
        steam_client_header_text = read(os.path.join(ROOT_DIR, "dll", "dll", "steam_client.h"))

    issues = []
    root_owners = (
        "GBE_SharedDotaLobbyState lobby_state_",
        "std::recursive_mutex lobby_mutex_",
        "dota_lobby_state::Store lobby_store_",
        "std::unique_ptr<LifecycleExecutor> lifecycle_executor_",
        "std::vector<dota_handler_registry::Entry> handler_registry_entries_",
        "RoleContext client_",
        "RoleContext server_",
    )
    for token in root_owners:
        if token not in composition_header_text:
            issues.append(f"gbe_dota_composition_root.h: CompositionRoot lost owned dependency '{token}'")

    explicit_dependencies = (
        (composition_header_text, "gbe_dota_composition_root.h", "RoleContext(\n        dota_lobby_state::Store &lobby_store,\n        RoleDependencies dependencies)"),
        (composition_header_text, "gbe_dota_composition_root.h", "ReconnectService(\n        GBE_DotaReconnectContextProvider &context_provider,\n        GBE_DotaReconnectDirectConnector &direct_connector,\n        GBE_DotaReconnectCallbackQueue &callback_queue)"),
        (coordinator_header_text, "steam_game_coordinator.h", "Steam_Game_Coordinator(class Settings *settings, class Networking *network, class Local_Storage *local_storage, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb, gbe::dota_lobby_state::Store &shared_lobby_store, gbe::dota_handler_registry::View handler_registry, gbe::dota_lifecycle::Executor &lifecycle_executor, bool is_server)"),
    )
    for source, filename, token in explicit_dependencies:
        if token not in source:
            issues.append(f"{filename}: GC service lost explicit constructor dependencies")

    if "require_dependency(lifecycle_executor_, \"lifecycle_executor\")" not in composition_source_text:
        issues.append("gbe_dota_composition_root.cpp: CompositionRoot must reject a missing lifecycle executor")

    production_owners = (
        "GBE_SharedDotaLobbyState dota_lobby_state",
        "dota_lobby_state::Store dota_lobby_store",
        "gbe::dota::RuntimeState dota_runtime_state",
        "GBE_DotaReconnectNetworkAdapter *dota_reconnect_adapter_client",
        "GBE_DotaReconnectNetworkAdapter *dota_reconnect_adapter_server",
        "gbe::dota_lifecycle::CoordinatorExecutor *dota_lifecycle_executor_client",
        "gbe::dota_lifecycle::CoordinatorExecutor *dota_lifecycle_executor_server",
    )
    for token in production_owners:
        if token not in steam_client_header_text:
            issues.append(f"steam_client.h: Steam_Client lost application-owned GC dependency '{token}'")

    lifecycle_tests = (
        "test_roots_isolate_owned_state",
        "test_client_assembly_uses_client_dependencies_only",
        "test_gameserver_assembly_uses_gameserver_dependencies_only",
        "test_delayed_work_expires_with_root_dependencies",
        "test_recreated_root_starts_without_previous_state",
    )
    for test_name in lifecycle_tests:
        if composition_test_text.count(test_name) < 2:
            issues.append(f"gbe_dota_composition_root_test.cpp: missing executed lifecycle regression {test_name}")

    # D.11.3: CompositionRoot is offline-only; production authority stays on Steam_Client.
    if "Offline-only" not in composition_header_text and "offline-only" not in composition_header_text:
        issues.append(
            "gbe_dota_composition_root.h: CompositionRoot must be explicitly marked offline-only"
        )
    steam_client_cpp = read(os.path.join(ROOT_DIR, "dll", "steam_client.cpp"))
    if "CompositionRoot" in steam_client_cpp or "gbe_dota_composition_root.h" in steam_client_cpp:
        issues.append(
            "steam_client.cpp: production assembly must not construct or include CompositionRoot"
        )
    architecture_text = read(os.path.join(ROOT_DIR, ".monkeycode", "docs", "ARCHITECTURE.md"))
    if "offline tests" not in architecture_text or "Steam_Client` as the runtime authority" not in architecture_text:
        issues.append(
            "ARCHITECTURE.md: must document CompositionRoot as offline-only and Steam_Client as runtime authority"
        )
    forbidden_doc_claims = (
        "CompositionRoot is injected into production",
        "production injects CompositionRoot",
        "CompositionRoot owns production GC assembly",
        "production CompositionRoot injection",
    )
    docs_root = os.path.join(ROOT_DIR, ".monkeycode", "docs")
    if os.path.isdir(docs_root):
        for root, _dirs, files in os.walk(docs_root):
            for name in files:
                if not name.endswith(".md"):
                    continue
                path = os.path.join(root, name)
                text = read(path)
                for claim in forbidden_doc_claims:
                    if claim in text:
                        rel = os.path.relpath(path, ROOT_DIR)
                        issues.append(
                            f"{rel}: docs must not claim CompositionRoot production injection ({claim})"
                        )
    return issues


def audit_mutable_gc_global_state(sources=None):
    """Reject mutable GC namespace/file static state outside narrow locators."""
    if sources is None:
        sources = {os.path.basename(path): read(path) for path in GC_TUS}

    issues = []
    declaration = re.compile(
        r"^\s*(?P<static>static\s+)?(?P<type>(?:(?:const|constexpr)\s+)?[^(){};=]+?)"
        r"\s+[&*]*\s*(?P<name>[A-Za-z_]\w*)\s*(?:=|\{|;)"
    )
    for filename, source_text in sources.items():
        brace_depth = 0
        for line_no, line in enumerate(strip_comments(source_text).splitlines(), 1):
            stripped = line.strip()
            match = declaration.match(line) if brace_depth == 0 else None
            brace_depth += line.count("{") - line.count("}")
            if not match:
                continue
            if stripped.startswith(("using ", "typedef ", "struct ", "class ", "enum ", "namespace ", "extern ")):
                continue
            declaration_text = match.group(0)
            if "const" in match.group("type").split() or "constexpr" in declaration_text.split():
                continue
            if not match.group("static") and line[:1].isspace():
                continue
            symbol = match.group("name")
            if (filename, symbol) in MUTABLE_GC_GLOBAL_ALLOWLIST:
                continue
            storage = "static" if match.group("static") else "global"
            issues.append(f"{filename}:{line_no}: mutable GC {storage} state {symbol} is not allowlisted")
    return issues


def audit_concurrency_ownership_contract():
    """Keep the P11.1 state-owner and lock-boundary contract complete."""
    if not os.path.exists(CONCURRENCY_OWNERSHIP_MD):
        return ["concurrency-ownership.md: missing P11.1 ownership contract"]

    contract_text = read(CONCURRENCY_OWNERSHIP_MD)
    issues = [
        f"concurrency-ownership.md: missing required boundary {term}"
        for term in CONCURRENCY_OWNERSHIP_TERMS
        if f"`{term}`" not in contract_text and term not in contract_text
    ]
    callsystem_text = read(os.path.join(ROOT_DIR, "dll", "callsystem.cpp"))
    for forbidden in ("global_mutex.unlock()", "global_mutex.lock()"):
        if forbidden in callsystem_text:
            issues.append(f"callsystem.cpp: raw {forbidden} bypasses the owned callback lock boundary")

    steam_client_text = read(os.path.join(ROOT_DIR, "dll", "steam_client.cpp"))
    run_callbacks_pos = steam_client_text.find("void Steam_Client::RunCallbacks(")
    run_callbacks_end = steam_client_text.find("\n}", run_callbacks_pos)
    run_callbacks_text = steam_client_text[run_callbacks_pos:run_callbacks_end]
    if "std::unique_lock<std::recursive_mutex> lock(global_mutex);" not in run_callbacks_text:
        issues.append("steam_client.cpp: RunCallbacks must own global_mutex through unique_lock")
    if "runCallResults(lock)" not in run_callbacks_text:
        issues.append("steam_client.cpp: RunCallbacks must pass its owned lock to callback delivery")

    serialized_text = read(os.path.join(ROOT_DIR, "dll", "steam_networking_socketsserialized.cpp"))
    serialized_header_text = read(os.path.join(ROOT_DIR, "dll", "dll", "steam_networking_socketsserialized.h"))
    if "GBE_DotaSerializedConnectionSynchronizer dota_connection_synchronizer;" not in serialized_header_text:
        issues.append("steam_networking_socketsserialized.h: serialized connection state lacks an instance synchronization owner")
    post_pos = serialized_text.find("void Steam_Networking_Sockets_Serialized::PostConnectionStateMsg(")
    process_lock_pos = serialized_text.find("std::unique_lock<std::recursive_mutex> lock(global_mutex);", post_pos)
    prepare_pos = serialized_text.find("GBE_PrepareDotaReconnectPostConnectionState(")
    instance_lock_pos = serialized_text.rfind("dota_connection_synchronizer.acquire()", process_lock_pos, prepare_pos)
    unlock_pos = serialized_text.find("lock.unlock();", prepare_pos)
    effects_pos = serialized_text.find("GBE_ExecuteDotaReconnectPostEffects(", unlock_pos)
    if min(post_pos, instance_lock_pos, process_lock_pos, prepare_pos, unlock_pos, effects_pos) < 0 or not process_lock_pos < instance_lock_pos < prepare_pos < unlock_pos < effects_pos:
        issues.append("steam_networking_socketsserialized.cpp: reconnect process-lock/instance-lock/prepare/process-unlock/effects order is missing")

    stress_path = os.path.join(ROOT_DIR, "tools", "gbe_dota_concurrency_stress_test", "gbe_dota_concurrency_stress_test.cpp")
    stress_text = read(stress_path) if os.path.exists(stress_path) else ""
    for required in (
        "store.snapshot()",
        "store.publish_if_generation_current_or_newer(",
        "store.compare_update(",
        "store.clear()",
        "gbe::dota_reconnect::build_context(",
        "torn_snapshots.load() == 0u",
        "stale_mutator_calls.load() == 0u",
        # D.11.2: queue + deferred Slot + generation interleave
        "test_queue_deferred_generation_interleave",
        "consume_deferred(",
        "deferred_stale",
        "deferred_current",
    ):
        if required not in stress_text:
            issues.append(f"gbe_dota_concurrency_stress_test.cpp: missing P11.4 boundary {required}")
    shell_text = read(os.path.join(ROOT_DIR, "tools", "run_gc_offline_tests.sh"))
    premake_text = read(os.path.join(ROOT_DIR, "premake5.lua"))
    for owner, text in (("run_gc_offline_tests.sh", shell_text), ("premake5.lua", premake_text)):
        if "gbe_dota_concurrency_stress_test" not in text:
            issues.append(f"{owner}: missing P11.4 concurrency stress test wiring")

    tsan_path = os.path.join(ROOT_DIR, "tools", "run_gc_tsan_tests.sh")
    tsan_text = read(tsan_path) if os.path.exists(tsan_path) else ""
    workflow_text = read(os.path.join(ROOT_DIR, ".github", "workflows", "emu-pull-request.yml"))
    for required in (
        "-fsanitize=thread",
        "halt_on_error=1:exitcode=66",
        "gbe_dota_reconnect_network_test",
        "gbe_dota_concurrency_stress_test",
    ):
        if required not in tsan_text:
            issues.append(f"run_gc_tsan_tests.sh: missing P11.5 TSAN boundary {required}")
    for required in ("gc-tsan:", "CXX: clang++", "bash tools/run_gc_tsan_tests.sh"):
        if required not in workflow_text:
            issues.append(f"emu-pull-request.yml: missing P11.5 TSAN CI boundary {required}")

    store_test_text = read(os.path.join(ROOT_DIR, "tools", "gbe_dota_lobby_state_store_test", "gbe_dota_lobby_state_store_test.cpp"))
    reconnect_test_text = read(os.path.join(ROOT_DIR, "tools", "gbe_dota_reconnect_network_test", "gbe_dota_reconnect_network_test.cpp"))
    for required in (
        "P11-A accepted generation history reaches its maximal linearization point",
        "P11-A accepted final-generation updates linearize exactly once",
    ):
        if required not in store_test_text:
            issues.append(f"gbe_dota_lobby_state_store_test.cpp: missing P11.6 property {required}")
    for required in (
        "P11-B network fake runs outside the store lock",
        "P11-B callback fake runs outside the store lock",
        "P11-C mutation of one serialized instance cannot change another instance",
        "P11-C second instance keeps an independent dedup history",
    ):
        if required not in reconnect_test_text:
            issues.append(f"gbe_dota_reconnect_network_test.cpp: missing P11.6 property {required}")
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
    dispatch_issues, total_entries, high_risk_entries = audit_post_login_dispatch(read(POST_LOGIN_REGISTRY_CPP))
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
    print("AUDIT 5b: gbe_dota_gc_internal.h slim boundary")
    print("=" * 70)
    print("  Action: keep free equip, networking/reconnect, payload-lobby, and locator out of gc_internal.")
    gc_internal_slim_issues = audit_gc_internal_slim_boundary()
    if not gc_internal_slim_issues:
        print("  gc_internal remains free of equip/networking/payload-lobby/locator re-exports")
    else:
        for issue in gc_internal_slim_issues:
            print(f"  {issue}")
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
    lifecycle_ownership_issues.extend(audit_retired_lifecycle_transition_layers())
    if not lifecycle_ownership_issues:
        print(f"  All {len(LIFECYCLE_SIDE_EFFECT_APIS)} lifecycle side-effect APIs remain owned by {LIFECYCLE_EXECUTOR_OWNER}; retired entrypoints and manual fallbacks remain absent")
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
    print("AUDIT 10b: Store write discipline (no bare publish/update in production)")
    print("=" * 70)
    print("  Action: production dll must use generation-gated Store writes only.")
    store_write_discipline_issues = audit_store_write_discipline()
    if not store_write_discipline_issues:
        print("  (none) - production paths avoid bare Store::publish / Store::update")
    else:
        for issue in store_write_discipline_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 11: Concurrency ownership contract")
    print("=" * 70)
    print("  Action: keep GC state owners, synchronization domains, and async boundaries explicit.")
    concurrency_ownership_issues = audit_concurrency_ownership_contract()
    if not concurrency_ownership_issues:
        print(f"  All {len(CONCURRENCY_OWNERSHIP_TERMS)} required P11.1 ownership boundaries are documented")
    else:
        for issue in concurrency_ownership_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 12: Retired reconnect transition layers")
    print("=" * 70)
    print("  Action: keep reconnect source mapping canonical and serialized state generation-scoped.")
    reconnect_transition_issues = audit_retired_reconnect_transition_layers()
    if not reconnect_transition_issues:
        print(f"  All {len(RETIRED_RECONNECT_TRANSITION_SYMBOLS)} retired reconnect symbols remain absent; serialized state remains generation-scoped")
    else:
        for issue in reconnect_transition_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 13: Retired shared lobby compatibility layers")
    print("=" * 70)
    print("  Action: keep production shared lobby access on the canonical Store contract.")
    shared_lobby_compatibility_issues = audit_retired_shared_lobby_compatibility_layers()
    if not shared_lobby_compatibility_issues:
        print("  All 10 retired shared lobby facades and the mutable backing global remain absent")
    else:
        for issue in shared_lobby_compatibility_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 14: Canonical GC architecture boundaries")
    print("=" * 70)
    print("  Action: keep handler effects, post-login dispatch, reconnect mapping, and shared state on canonical owners.")
    architecture_boundary_issues = audit_architecture_boundaries()
    if not architecture_boundary_issues:
        print("  Handler seams, the typed registry, reconnect adapters, and shared lobby Store boundaries remain canonical")
    else:
        for issue in architecture_boundary_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 15: Composition root lifecycle")
    print("=" * 70)
    print("  Action: keep coordinators inside the lifetime of their GC services and infrastructure.")
    composition_root_lifecycle_issues = audit_composition_root_lifecycle()
    if not composition_root_lifecycle_issues:
        print("  Client and gameserver GC construction and destruction follow the declared dependency order")
    else:
        for issue in composition_root_lifecycle_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 16: Mutable GC global state")
    print("=" * 70)
    print("  Action: keep mutable GC business state on application-owned instances.")
    mutable_gc_global_issues = audit_mutable_gc_global_state()
    if not mutable_gc_global_issues:
        print(f"  Only {len(MUTABLE_GC_GLOBAL_ALLOWLIST)} non-owning compatibility locators remain; immutable tables and functions are accepted")
    else:
        for issue in mutable_gc_global_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 17: Layered GC CI gates")
    print("=" * 70)
    print("  Action: keep fast, production, and sanitizer PR checks separate and blocking.")
    layered_ci_issues = audit_layered_ci_gates()
    if not layered_ci_issues:
        print("  Fast offline/audit/diff, Windows/Linux release, and Clang TSAN layers remain blocking")
    else:
        for issue in layered_ci_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 18: Lifecycle transition gates")
    print("=" * 70)
    print("  Action: keep core lifecycle handlers and coordinators behind typed transition effects.")
    lifecycle_transition_gate_issues = audit_lifecycle_transition_gates()
    if not lifecycle_transition_gate_issues:
        print(f"  All {len(LIFECYCLE_TRANSITION_GATES)} migrated lifecycle owners retain their transition and effect gates")
    else:
        for issue in lifecycle_transition_gate_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 19: Architecture investment inputs")
    print("=" * 70)
    print("  Action: keep Actor, formal-model, and model-CI decisions tied to repeatable evidence.")
    architecture_investment_input_issues = audit_architecture_investment_inputs()
    if not architecture_investment_input_issues:
        print("  Versioned concurrency, state-machine coverage, defect, and CI-time inputs are complete")
    else:
        for issue in architecture_investment_input_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 20: Handler responsibility boundaries")
    print("=" * 70)
    print("  Action: keep handlers on dispatch, parse, mapping, orchestration, response adaptation, and logging.")
    handler_responsibility_issues, handler_responsibilities = audit_handler_responsibility_boundaries()
    if not handler_responsibility_issues:
        print(f"  All {sum(handler_responsibilities.values())} accepted direct handler compatibility operations remain at or below baseline")
    else:
        for issue in handler_responsibility_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 21: State and effect ownership")
    print("=" * 70)
    print("  Action: keep planners pure, lifecycle effects coordinated, shared state stored, and reconnect network calls adapted.")
    state_effect_ownership_issues = audit_state_effect_ownership()
    if not state_effect_ownership_issues:
        print(f"  All {len(PURE_DOTA_PLANNER_FILES)} pure planners and canonical lifecycle/network owners retain their boundaries")
    else:
        for issue in state_effect_ownership_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 22: Dependency and object lifecycle")
    print("=" * 70)
    print("  Action: keep mutable state application-owned, service dependencies explicit, and roots isolated.")
    dependency_object_lifecycle_issues = audit_dependency_object_lifecycle()
    if not dependency_object_lifecycle_issues:
        print("  Production owners, explicit service dependencies, and lifecycle isolation regressions remain canonical")
    else:
        for issue in dependency_object_lifecycle_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 23: Core state machine boundaries")
    print("=" * 70)
    print("  Action: keep lifecycle, generation, and reconnect transitions on pure canonical functions.")
    core_state_machine_issues = audit_core_state_machine_boundaries()
    if not core_state_machine_issues:
        print("  Lifecycle transitions, generation advancement, and reconnect dedup remain on canonical owners")
    else:
        for issue in core_state_machine_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 24: Generation-scoped asynchronous work")
    print("=" * 70)
    print("  Action: capture generation when queueing work and validate it at the final execution boundary.")
    async_generation_issues = audit_async_generation_safety()
    if not async_generation_issues:
        print("  Delayed messages, deferred slots, reconnect callbacks, and runtime updates retain generation gates")
    else:
        for issue in async_generation_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 25: High-risk test credibility")
    print("=" * 70)
    print("  Action: retain pure logic, executor, production-path fake, property, and differential coverage.")
    test_credibility_issues = audit_test_credibility()
    if not test_credibility_issues:
        print("  Lifecycle and reconnect high-risk tests remain defined, executed, and wired into the offline gate")
    else:
        for issue in test_credibility_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 26: CI failure localization")
    print("=" * 70)
    print("  Action: keep offline, production, and TSAN failures independently named, blocking, and fail-fast.")
    ci_failure_localization_issues = audit_ci_failure_localization()
    if not ci_failure_localization_issues:
        print("  Four blocking PR jobs and their fail-fast scripts retain stable diagnostic boundaries")
    else:
        for issue in ci_failure_localization_issues:
            print(f"  {issue}")
    print()

    print("=" * 70)
    print("AUDIT 27: Architecture investment boundaries")
    print("=" * 70)
    print("  Action: derive Actor, formal-model, and model-CI decisions from versioned evidence.")
    architecture_investment_boundary_issues = audit_architecture_investment_boundaries()
    if not architecture_investment_boundary_issues:
        print("  JSON evidence, gate documentation, and task decisions remain aligned with objective thresholds")
    else:
        for issue in architecture_investment_boundary_issues:
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
    print(f"  GC internal slim boundary issues:    {len(gc_internal_slim_issues)}")
    print(f"  Source-list inclusion issues:        {len(source_list_issues)}")
    print(f"  Handler side-effect seam issues:     {len(side_effect_issues)}")
    print(f"  High-risk reason inventory issues:   {len(reason_issues)}")
    print(f"  Lifecycle ownership issues:          {len(lifecycle_ownership_issues)}")
    print(f"  Shared lobby global access issues:   {len(shared_lobby_global_issues)}")
    print(f"  Store write discipline issues:       {len(store_write_discipline_issues)}")
    print(f"  Concurrency ownership issues:        {len(concurrency_ownership_issues)}")
    print(f"  Reconnect transition-layer issues:   {len(reconnect_transition_issues)}")
    print(f"  Shared lobby compatibility issues:   {len(shared_lobby_compatibility_issues)}")
    print(f"  Architecture boundary issues:        {len(architecture_boundary_issues)}")
    print(f"  Composition root lifecycle issues:   {len(composition_root_lifecycle_issues)}")
    print(f"  Mutable GC global state issues:       {len(mutable_gc_global_issues)}")
    print(f"  Layered CI gate issues:               {len(layered_ci_issues)}")
    print(f"  Lifecycle transition gate issues:    {len(lifecycle_transition_gate_issues)}")
    print(f"  Architecture investment issues:      {len(architecture_investment_input_issues)}")
    print(f"  Handler responsibility issues:       {len(handler_responsibility_issues)}")
    print(f"  State and effect ownership issues:    {len(state_effect_ownership_issues)}")
    print(f"  Dependency/object lifecycle issues:   {len(dependency_object_lifecycle_issues)}")
    print(f"  Core state machine issues:            {len(core_state_machine_issues)}")
    print(f"  Async generation safety issues:       {len(async_generation_issues)}")
    print(f"  High-risk test credibility issues:    {len(test_credibility_issues)}")
    print(f"  CI failure localization issues:       {len(ci_failure_localization_issues)}")
    print(f"  Architecture investment boundary issues: {len(architecture_investment_boundary_issues)}")

    if zombies or underexposed or mismatches or dispatch_issues or template_blob_issues or gc_internal_slim_issues or source_list_issues or side_effect_issues or reason_issues or lifecycle_ownership_issues or shared_lobby_global_issues or store_write_discipline_issues or concurrency_ownership_issues or reconnect_transition_issues or shared_lobby_compatibility_issues or architecture_boundary_issues or composition_root_lifecycle_issues or mutable_gc_global_issues or layered_ci_issues or lifecycle_transition_gate_issues or architecture_investment_input_issues or handler_responsibility_issues or state_effect_ownership_issues or dependency_object_lifecycle_issues or core_state_machine_issues or async_generation_issues or test_credibility_issues or ci_failure_localization_issues or architecture_investment_boundary_issues:
        sys.exit(1)


if __name__ == "__main__":
    main()
