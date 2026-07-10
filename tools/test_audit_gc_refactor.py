#!/usr/bin/env python3
"""Focused regression tests for GC refactor audit helpers."""

import unittest

import _audit_gc_refactor as audit


HEADER = """
enum class Reason : std::uint8_t {
    Unknown = 0,
    Alpha,
    Beta,
};

constexpr const char *describe_reason(Reason reason) {
    switch (reason) {
        case Reason::Unknown: return "unknown";
        case Reason::Alpha: return "alpha";
        case Reason::Beta: return "beta";
    }
    return "unknown";
}
"""

FOCUSED = """
{diagnostic::Reason::Unknown, "unknown"},
{diagnostic::Reason::Alpha, "alpha"},
{diagnostic::Reason::Beta, "beta"},
"""


class DiagnosticReasonInventoryAuditTest(unittest.TestCase):
    def audit(self, header=HEADER, focused=FOCUSED):
        return audit.audit_diagnostic_reason_inventory(header, focused)[0]

    def test_accepts_complete_unique_inventory(self):
        self.assertEqual([], self.audit())

    def test_rejects_missing_serializer_mapping(self):
        header = HEADER.replace('        case Reason::Beta: return "beta";\n', "")
        self.assertIn(
            "diagnostic Reason::Beta: missing stable describe_reason mapping",
            self.audit(header=header),
        )

    def test_rejects_serializer_mapping_without_enum(self):
        header = HEADER.replace(
            '        case Reason::Beta: return "beta";',
            '        case Reason::Beta: return "beta";\n        case Reason::Extra: return "extra";',
        )
        self.assertIn(
            "diagnostic Reason::Extra: mapping has no enum entry",
            self.audit(header=header),
        )

    def test_rejects_duplicate_serialized_value(self):
        header = HEADER.replace('Reason::Beta: return "beta"', 'Reason::Beta: return "alpha"')
        self.assertIn(
            "diagnostic reason value 'alpha': duplicate mapping for Alpha, Beta",
            self.audit(header=header),
        )

    def test_rejects_missing_focused_coverage(self):
        focused = FOCUSED.replace('{diagnostic::Reason::Beta, "beta"},\n', "")
        self.assertIn(
            "diagnostic Reason::Beta: missing from focused serialization inventory",
            self.audit(focused=focused),
        )

    def test_rejects_focused_value_drift(self):
        focused = FOCUSED.replace('{diagnostic::Reason::Beta, "beta"}', '{diagnostic::Reason::Beta, "changed"}')
        self.assertIn(
            "diagnostic Reason::Beta: focused value 'changed' does not match 'beta'",
            self.audit(focused=focused),
        )


class ConcurrencyOwnershipAuditTest(unittest.TestCase):
    def test_contract_terms_cover_p11_state_and_follow_up_boundaries(self):
        required = set(audit.CONCURRENCY_OWNERSHIP_TERMS)
        self.assertTrue({
            "Shared lobby store",
            "Recent reconnect context",
            "Serialized connection state",
            "Callback queue",
            "Delayed reconnect callback",
            "Delayed GC message",
            "Deferred lifecycle slot",
            "P11.2",
            "P11.3",
        }.issubset(required))

    def test_production_lock_boundary_audit_passes(self):
        self.assertEqual([], audit.audit_concurrency_ownership_contract())

    def test_tsan_gate_audit_passes(self):
        issues = audit.audit_concurrency_ownership_contract()
        self.assertFalse([issue for issue in issues if "P11.5" in issue or "TSAN" in issue])


class CompositionRootLifecycleAuditTest(unittest.TestCase):
    STEAM_CLIENT = """
    Steam_Client::Steam_Client()
    {
        steam_networking_sockets = new Steam_Networking_Sockets();
        dota_reconnect_adapter_client = new GBE_DotaReconnectNetworkAdapter();
        steam_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();
        steam_game_coordinator = new Steam_Game_Coordinator();
        steam_gameserver_networking_sockets = new Steam_Networking_Sockets();
        dota_reconnect_adapter_server = new GBE_DotaReconnectNetworkAdapter();
        steam_gameserver_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();
        steam_gameserver_game_coordinator = new Steam_Game_Coordinator();
    }
    Steam_Client::~Steam_Client()
    {
        DEL_INST(steam_gameserver_game_coordinator);
        DEL_INST(steam_gameserver_networking_sockets_serialized);
        DEL_INST(dota_reconnect_adapter_server);
        DEL_INST(steam_gameserver_networking_sockets);
        DEL_INST(steam_game_coordinator);
        DEL_INST(steam_networking_sockets_serialized);
        DEL_INST(dota_reconnect_adapter_client);
        DEL_INST(steam_networking_sockets);
    }
    """

    def test_accepts_dependency_order_for_both_roles(self):
        self.assertEqual([], audit.audit_composition_root_lifecycle(self.STEAM_CLIENT))

    def test_rejects_coordinator_before_services(self):
        source = self.STEAM_CLIENT.replace(
            "        steam_networking_sockets = new Steam_Networking_Sockets();\n"
            "        dota_reconnect_adapter_client = new GBE_DotaReconnectNetworkAdapter();\n"
            "        steam_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();\n"
            "        steam_game_coordinator = new Steam_Game_Coordinator();",
            "        steam_game_coordinator = new Steam_Game_Coordinator();\n"
            "        steam_networking_sockets = new Steam_Networking_Sockets();\n"
            "        dota_reconnect_adapter_client = new GBE_DotaReconnectNetworkAdapter();\n"
            "        steam_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();",
        )
        self.assertIn(
            "steam_client.cpp: client GC construction must order direct sockets, reconnect adapter, serialized services, then coordinator",
            audit.audit_composition_root_lifecycle(source),
        )

    def test_rejects_service_destroyed_before_coordinator(self):
        source = self.STEAM_CLIENT.replace(
            "        DEL_INST(steam_gameserver_game_coordinator);\n"
            "        DEL_INST(steam_gameserver_networking_sockets_serialized);\n"
            "        DEL_INST(dota_reconnect_adapter_server);",
            "        DEL_INST(steam_gameserver_networking_sockets_serialized);\n"
            "        DEL_INST(dota_reconnect_adapter_server);\n"
            "        DEL_INST(steam_gameserver_game_coordinator);",
        )
        self.assertIn(
            "steam_client.cpp: gameserver GC destruction must order coordinator, serialized services, reconnect adapter, then direct sockets",
            audit.audit_composition_root_lifecycle(source),
        )


class RetiredLifecycleTransitionLayerAuditTest(unittest.TestCase):
    def test_accepts_registry_only_lifecycle_dispatch(self):
        sources = {
            "steam_game_coordinator.h": "bool GBE_HandleDotaCustomGameLifecycleRequest();",
            "gbe_dota_match_handlers.cpp": "void GBE_HandleDotaDirect7034Request() {}",
            "gbe_dota_post_login_handlers.cpp": "return GBE_DispatchDotaPostLoginRequest(context);",
        }
        self.assertEqual([], audit.audit_retired_lifecycle_transition_layers(sources))

    def test_rejects_retired_handler_and_manual_fallback(self):
        sources = {
            "steam_game_coordinator.h": "bool GBE_HandleDotaCustomGameReadyUpRequest();",
            "gbe_dota_match_handlers.cpp": "",
            "gbe_dota_post_login_handlers.cpp": "if (request_emsg == 7070u) return true;",
        }
        issues = audit.audit_retired_lifecycle_transition_layers(sources)
        self.assertIn(
            "steam_game_coordinator.h: retired lifecycle handler GBE_HandleDotaCustomGameReadyUpRequest returned",
            issues,
        )
        self.assertIn(
            "gbe_dota_post_login_handlers.cpp: lifecycle emsg 7070u bypasses the typed registry",
            issues,
        )


class RetiredReconnectTransitionLayerAuditTest(unittest.TestCase):
    def test_accepts_canonical_mapping_and_generation_state(self):
        sources = {
            "gbe_dota_reconnect_context.cpp": "Source source_from_shared_lobby_snapshot(const Shared &snapshot);",
            "gbe_dota_serialized_connection_state.h": """
                struct GBE_DotaSerializedConnectionState {
                    std::uint64_t generation{};
                    void begin_generation(std::uint64_t current_generation);
                };
            """,
        }
        self.assertEqual([], audit.audit_retired_reconnect_transition_layers(sources))

    def test_rejects_retired_mapping_and_lobby_id_state(self):
        sources = {
            "gbe_dota_reconnect_context.cpp": "Source source_from_shared_snapshot(const Snapshot &snapshot);",
            "gbe_dota_serialized_connection_state.h": """
                struct GBE_DotaSerializedConnectionState {
                    std::uint64_t lobby_id{};
                    void begin_lobby(std::uint64_t lobby_id, std::uint64_t generation = 0);
                    void begin_generation(std::uint64_t current_generation = 0);
                };
            """,
        }
        issues = audit.audit_retired_reconnect_transition_layers(sources)
        self.assertIn(
            "gbe_dota_reconnect_context.cpp: retired reconnect transition symbol source_from_shared_snapshot returned",
            issues,
        )
        self.assertIn(
            "gbe_dota_serialized_connection_state.h: serialized reconnect state restored lobby_id compatibility state",
            issues,
        )
        self.assertIn(
            "gbe_dota_serialized_connection_state.h: serialized reconnect state restored begin_lobby compatibility API",
            issues,
        )
        self.assertIn(
            "gbe_dota_serialized_connection_state.h: begin_generation restored a default generation",
            issues,
        )


class RetiredSharedLobbyCompatibilityLayerAuditTest(unittest.TestCase):
    def test_accepts_canonical_store_access(self):
        sources = {
            "gbe_dota_gc_internal.h": "gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore();",
            "gbe_dota_lobby_handlers.cpp": "const auto shared = GBE_SharedLobbyStore().snapshot();",
        }
        self.assertEqual([], audit.audit_retired_shared_lobby_compatibility_layers(sources))

    def test_rejects_projection_facade_and_mutable_backing_global(self):
        sources = {
            "gbe_dota_gc_internal.h": "GBE_SharedDotaLobbyState GBE_GetSharedDotaLobbyStateSnapshot();",
            "steam_game_coordinator.cpp": "GBE_SharedDotaLobbyState GBE_shared_dota_lobby_state;",
        }
        issues = audit.audit_retired_shared_lobby_compatibility_layers(sources)
        self.assertIn(
            "gbe_dota_gc_internal.h: retired shared lobby compatibility symbol GBE_GetSharedDotaLobbyStateSnapshot returned",
            issues,
        )
        self.assertIn(
            "steam_game_coordinator.cpp: retired shared lobby compatibility symbol GBE_shared_dota_lobby_state returned",
            issues,
        )


class HandlerSideEffectSeamAuditTest(unittest.TestCase):
    def test_accepts_handler_using_approved_executor_seam(self):
        sources = {
            "gbe_dota_lobby_handlers.cpp": "return lifecycle_executor.execute(context);",
        }
        issues, call_count, baseline_count = audit.audit_handler_side_effect_seams(
            source_texts=sources,
            baseline={},
        )
        self.assertEqual([], issues)
        self.assertEqual(0, call_count)
        self.assertEqual(0, baseline_count)

    def test_rejects_new_direct_high_risk_side_effect(self):
        sources = {
            "gbe_dota_lobby_handlers.cpp": "GBE_PublishSharedDotaLobbyState(lobby);",
        }
        issues, _, _ = audit.audit_handler_side_effect_seams(
            source_texts=sources,
            baseline={},
        )
        self.assertIn(
            "gbe_dota_lobby_handlers.cpp: new high-risk side-effect call to GBE_PublishSharedDotaLobbyState requires an approved seam or explicit baseline entry",
            issues,
        )


class ArchitectureBoundaryAuditTest(unittest.TestCase):
    def test_accepts_canonical_architecture_owners(self):
        sources = {
            "steam_game_coordinator.cpp": """
                registry::View Steam_Game_Coordinator::GBE_ProductionDotaHandlerRegistry() {
                    static const registry::Entry kTable[] = {};
                    return {kTable, 0};
                }
                bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest(const Context &context) {
                    return registry::find_entry(handler_registry.entries, handler_registry.size, context.inner_emsg, context.path);
                }
            """,
            "gbe_dota_post_login_handlers.cpp": "return GBE_DispatchDotaPostLoginRequest(context);",
            "gbe_dota_template_replay_handlers.cpp": "switch (request_emsg) { default: return false; }",
            "gbe_dota_reconnect_context.cpp": """
                Source source_from_context(const GBE_DotaReconnectContext &context) {
                    Source source;
                    source.generation = context.generation;
                    source.lobby_id = context.lobby_id;
                    return source;
                }
            """,
            "gbe_dota_gc_payload_helpers.cpp": "return dota_reconnect::build_context(source, context);",
            "gbe_dota_lobby_handlers.cpp": "const auto shared = GBE_SharedLobbyStore().snapshot();",
        }
        self.assertEqual([], audit.audit_architecture_boundaries(sources))

    def test_rejects_registry_table_owned_by_dispatcher(self):
        sources = {
            "steam_game_coordinator.cpp": """
                bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest(const Context &context) {
                    static const registry::Entry kTable[] = {};
                    return registry::find_entry(kTable, 0, context.inner_emsg, context.path);
                }
            """,
        }
        issues = audit.audit_architecture_boundaries(sources)
        self.assertIn(
            "steam_game_coordinator.cpp: post-login dispatcher owns a registry table instead of consuming the injected view",
            issues,
        )

    def test_rejects_global_store_access_in_coordinator_business_path(self):
        sources = {
            "gbe_dota_lobby_handlers.cpp": "const auto shared = GBE_GetSharedDotaLobbyStateStore().snapshot();",
        }
        self.assertIn(
            "gbe_dota_lobby_handlers.cpp: coordinator business path bypasses the injected shared lobby Store",
            audit.audit_architecture_boundaries(sources),
        )

    def test_rejects_parallel_post_login_registry_and_switch(self):
        sources = {
            "steam_game_coordinator.cpp": """
                bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest(const Context &context) {
                    static const registry::Entry kTable[] = {};
                    return registry::find_entry(kTable, 0, context.inner_emsg, context.path);
                }
            """,
            "gbe_dota_post_login_handlers.cpp": """
                static const registry::Entry kFallbackTable[] = {};
                switch (request_emsg) { default: return false; }
            """,
        }
        issues = audit.audit_architecture_boundaries(sources)
        self.assertIn(
            "gbe_dota_post_login_handlers.cpp: parallel typed post-login registry returned outside steam_game_coordinator.cpp",
            issues,
        )
        self.assertIn(
            "gbe_dota_post_login_handlers.cpp: post-login message switch bypasses the typed registry",
            issues,
        )

    def test_rejects_reconnect_field_mapping_outside_canonical_owner(self):
        sources = {
            "gbe_dota_reconnect_context.cpp": "Source source_from_context(const GBE_DotaReconnectContext &context);",
            "gbe_dota_gc_payload_helpers.cpp": """
                GBE_DotaReconnectSource source;
                source.generation = snapshot.generation;
                source.lobby_id = snapshot.lobby_id;
            """,
        }
        issues = audit.audit_architecture_boundaries(sources)
        self.assertIn(
            "gbe_dota_gc_payload_helpers.cpp: reconnect source field generation is mapped outside gbe_dota_reconnect_context.cpp",
            issues,
        )
        self.assertIn(
            "gbe_dota_gc_payload_helpers.cpp: reconnect source field lobby_id is mapped outside gbe_dota_reconnect_context.cpp",
            issues,
        )

    def test_rejects_retired_shared_lobby_direct_access(self):
        sources = {
            "gbe_dota_lobby_handlers.cpp": "const auto shared = GBE_GetSharedDotaLobbyStateSnapshot();",
        }
        self.assertIn(
            "gbe_dota_lobby_handlers.cpp: retired shared lobby compatibility symbol GBE_GetSharedDotaLobbyStateSnapshot returned",
            audit.audit_architecture_boundaries(sources),
        )


class LayeredCiGateAuditTest(unittest.TestCase):
    WORKFLOW = """
        emu-win-release:
          uses: "./.github/workflows/emu-build-all-win.yml"
          with:
            matrix_prj: '["api_experimental"]'
            matrix_arch: '["x64"]'
            matrix_cfg: '["release"]'
            continue_on_error: false
        emu-linux-release:
          uses: "./.github/workflows/emu-build-all-linux.yml"
          with:
            matrix_prj: '["api_experimental"]'
            matrix_arch: '["x64"]'
            matrix_cfg: '["release"]'
            continue_on_error: false
        gc-verification:
          runs-on: "ubuntu-24.04"
          steps:
            - uses: actions/checkout@v6
              with:
                fetch-depth: 0
            - run: bash tools/run_gc_verification.sh --fast --base-sha "${{ github.event.pull_request.base.sha }}"
        gc-tsan:
          runs-on: "ubuntu-24.04"
          env:
            CXX: clang++
          run: bash tools/run_gc_tsan_tests.sh
    """
    VERIFICATION = """
        tools/run_gc_offline_tests.sh
        python3 tools/_audit_gc_refactor.py
        git diff --check "$BASE_SHA..HEAD"
    """
    OFFLINE = """
        python3 tools/test_audit_gc_refactor.py
        gbe_dota_reconnect_network_test
        gbe_dota_lobby_state_store_test
        gbe_dota_concurrency_stress_test
        gbe_dota_handler_registry_test
        gc_replay_test
    """
    TSAN = """
        -fsanitize=thread
        halt_on_error=1:exitcode=66
        gbe_dota_reconnect_network_test
        gbe_dota_concurrency_stress_test
    """

    def test_accepts_blocking_fast_production_and_tsan_layers(self):
        self.assertEqual(
            [],
            audit.audit_layered_ci_gates(
                self.WORKFLOW,
                self.VERIFICATION,
                self.OFFLINE,
                self.TSAN,
            ),
        )

    def test_rejects_full_fast_job_and_missing_production_or_tsan_boundaries(self):
        workflow = self.WORKFLOW.replace("--fast", "--full")
        workflow = workflow.replace("matrix_cfg: '[\"release\"]'", "matrix_cfg: '[\"debug\"]'", 1)
        workflow = workflow.replace("CXX: clang++", "CXX: c++")
        issues = audit.audit_layered_ci_gates(
            workflow,
            self.VERIFICATION.replace("python3 tools/_audit_gc_refactor.py", ""),
            self.OFFLINE,
            self.TSAN,
        )
        self.assertIn(
            "emu-pull-request.yml: fast GC layer must run run_gc_verification.sh --fast with the PR base SHA",
            issues,
        )
        self.assertIn(
            "emu-pull-request.yml: Windows production layer must build api_experimental x64 release with failures blocking",
            issues,
        )
        self.assertIn(
            "emu-pull-request.yml: TSAN layer must use Clang and run the dedicated sanitizer script",
            issues,
        )
        self.assertIn(
            "run_gc_verification.sh: fast layer is missing architecture audit execution",
            issues,
        )


if __name__ == "__main__":
    unittest.main()
