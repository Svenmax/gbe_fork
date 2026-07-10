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


if __name__ == "__main__":
    unittest.main()
