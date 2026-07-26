#!/usr/bin/env python3
"""Focused regression tests for GC refactor audit helpers."""

import unittest
import json

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


class InventorySectionHelperTest(unittest.TestCase):
    def test_returns_section_through_next_level_two_heading(self):
        text = "intro\n## 1. Alpha\nrow\n### detail\nstill alpha\n## 2. Beta\nother\n"
        self.assertEqual(
            "## 1. Alpha\nrow\n### detail\nstill alpha\n",
            audit.inventory_section(text, "## 1. Alpha"),
        )

    def test_returns_none_for_missing_heading(self):
        self.assertIsNone(audit.inventory_section("## 1. Alpha\n", "## 2. Beta"))

    def test_ignores_inline_heading_text_before_real_heading(self):
        text = "intro mentions ## 1. Alpha inline\n## 0. Before\nignore\n## 1. Alpha\nrow\n## 2. Beta\nother\n"
        self.assertEqual(
            "## 1. Alpha\nrow\n",
            audit.inventory_section(text, "## 1. Alpha"),
        )

    def test_ignores_same_prefix_longer_heading_before_real_heading(self):
        text = "## 1. Alpha extended\nwrong\n## 1. Alpha\nrow\n## 2. Beta\nother\n"
        self.assertEqual(
            "## 1. Alpha\nrow\n",
            audit.inventory_section(text, "## 1. Alpha"),
        )

    def test_accepts_heading_with_parenthetical_annotation(self):
        text = "## 1. Alpha（annotated）\nrow\n## 2. Beta\nother\n"
        self.assertEqual(
            "## 1. Alpha（annotated）\nrow\n",
            audit.inventory_section(text, "## 1. Alpha"),
        )


class MarkdownCellsHelperTest(unittest.TestCase):
    def test_trims_outer_pipes_and_cell_whitespace(self):
        self.assertEqual(["7091", "REGISTRY", "JoinChat"], audit.markdown_cells("| 7091 | REGISTRY | JoinChat |"))

    def test_accepts_rows_without_outer_pipes(self):
        self.assertEqual(["7091", "REGISTRY"], audit.markdown_cells(" 7091 | REGISTRY "))


class NumericMarkdownCellHelperTest(unittest.TestCase):
    def test_extracts_exact_numeric_cell(self):
        self.assertEqual("7091", audit.numeric_markdown_cell("7091"))

    def test_rejects_non_exact_numeric_cell_by_default(self):
        self.assertIsNone(audit.numeric_markdown_cell("emsg 7091"))

    def test_extracts_first_number_when_exact_is_false(self):
        self.assertEqual("7091", audit.numeric_markdown_cell("emsg 7091 conditional", exact=False))


class NumericMarkdownCellValuesHelperTest(unittest.TestCase):
    def test_extracts_multiple_numeric_values_in_order(self):
        self.assertEqual(["2536", "8744", "7091"], audit.numeric_markdown_cell_values("2536, 8744, 7091"))

    def test_returns_empty_list_when_no_values_exist(self):
        self.assertEqual([], audit.numeric_markdown_cell_values("template only"))

    def test_extracts_values_from_mixed_text(self):
        self.assertEqual(["2536", "8744"], audit.numeric_markdown_cell_values("emsg 2536 / fallback 8744"))


class ResolveEmsgTokenHelperTest(unittest.TestCase):
    def test_resolves_numeric_literal(self):
        self.assertEqual("7091", audit.resolve_emsg_token("7091", {}))

    def test_resolves_unsigned_numeric_literal(self):
        self.assertEqual("7091", audit.resolve_emsg_token("7091u", {}))

    def test_resolves_named_constant(self):
        self.assertEqual("7091", audit.resolve_emsg_token("GBE_kDotaJoinChat", {"GBE_kDotaJoinChat": "7091"}))

    def test_returns_none_for_unknown_named_token(self):
        self.assertIsNone(audit.resolve_emsg_token("GBE_kMissing", {}))


class TextBetweenMarkersHelperTest(unittest.TestCase):
    def test_returns_text_from_start_marker_to_before_end_marker(self):
        text = "prefix START body END suffix"
        self.assertEqual("START body ", audit.text_between_markers(text, "START", "END"))

    def test_returns_empty_string_when_start_marker_is_missing(self):
        self.assertEqual("", audit.text_between_markers("prefix END", "START", "END"))

    def test_returns_empty_string_when_end_marker_is_missing(self):
        self.assertEqual("", audit.text_between_markers("START body", "START", "END"))


class MarkerAppearsAfterHelperTest(unittest.TestCase):
    def test_returns_true_when_marker_follows_reference(self):
        self.assertTrue(audit.marker_appears_after("registry helper template", "helper", "registry"))

    def test_returns_false_when_marker_precedes_reference(self):
        self.assertFalse(audit.marker_appears_after("helper registry template", "helper", "registry"))

    def test_returns_false_when_marker_is_missing(self):
        self.assertFalse(audit.marker_appears_after("registry template", "helper", "registry"))

    def test_returns_false_when_reference_is_missing(self):
        self.assertFalse(audit.marker_appears_after("helper template", "helper", "registry"))


class ContainsAnyTokenHelperTest(unittest.TestCase):
    def test_returns_true_when_any_token_is_present(self):
        self.assertTrue(audit.contains_any_token("before beta after", ("alpha", "beta")))

    def test_returns_false_when_tokens_are_absent(self):
        self.assertFalse(audit.contains_any_token("before gamma after", ("alpha", "beta")))

    def test_returns_false_for_empty_token_collection(self):
        self.assertFalse(audit.contains_any_token("before beta after", ()))


class ContainsFunctionCallHelperTest(unittest.TestCase):
    def test_detects_function_call_with_whitespace(self):
        self.assertTrue(audit.contains_function_call("GBE_DoThing  (value);", "GBE_DoThing"))

    def test_returns_false_when_call_is_missing(self):
        self.assertFalse(audit.contains_function_call("GBE_DoThingValue;", "GBE_DoThing"))

    def test_escapes_symbol_text(self):
        self.assertTrue(audit.contains_function_call("GBE_DoThing_v1(value);", "GBE_DoThing_v1"))

    def test_requires_word_boundary_before_symbol(self):
        self.assertFalse(audit.contains_function_call("NotGBE_DoThing(value);", "GBE_DoThing"))


class ContainsWordTokenHelperTest(unittest.TestCase):
    def test_detects_whole_word_token(self):
        self.assertTrue(audit.contains_word_token("before GBE_DoThing after", "GBE_DoThing"))

    def test_rejects_token_inside_longer_word(self):
        self.assertFalse(audit.contains_word_token("before GBE_DoThingExtra after", "GBE_DoThing"))

    def test_escapes_token_text(self):
        self.assertTrue(audit.contains_word_token("emsg 7034 found", "7034"))


class RecordDuplicateHelperTest(unittest.TestCase):
    def test_first_value_is_seen_without_duplicate(self):
        seen = set()
        duplicates = set()
        audit.record_duplicate(seen, duplicates, "7091")
        self.assertEqual({"7091"}, seen)
        self.assertEqual(set(), duplicates)

    def test_repeated_value_is_recorded_as_duplicate(self):
        seen = {"7091"}
        duplicates = set()
        audit.record_duplicate(seen, duplicates, "7091")
        self.assertEqual({"7091"}, seen)
        self.assertEqual({"7091"}, duplicates)


class AppendDuplicateIssuesHelperTest(unittest.TestCase):
    def test_appends_duplicates_in_numeric_order(self):
        issues = []
        audit.append_duplicate_issues(issues, {"20", "3", "100"}, "prefix")
        self.assertEqual(["prefix 3", "prefix 20", "prefix 100"], issues)

    def test_empty_duplicates_do_not_append_issues(self):
        issues = ["existing"]
        audit.append_duplicate_issues(issues, set(), "prefix")
        self.assertEqual(["existing"], issues)


class AppendEmsgSetDiffIssueHelperTest(unittest.TestCase):
    def test_appends_diff_with_numeric_order_and_default_label(self):
        issues = []
        audit.append_emsg_set_diff_issue(issues, "prefix", {"100", "3"}, {"20", "4"})
        self.assertEqual(["prefix ['3', '100'] differ from expected ['4', '20']"], issues)

    def test_appends_diff_with_custom_expected_label(self):
        issues = []
        audit.append_emsg_set_diff_issue(issues, "prefix", {"100", "3"}, {"20", "4"}, "production kTable")
        self.assertEqual(["prefix ['3', '100'] differ from production kTable ['4', '20']"], issues)


class ExtractCaseEmsgsHelperTest(unittest.TestCase):
    def test_extracts_numeric_case_labels(self):
        body = "case 2536:\ncase 8744:\nbreak;"
        self.assertEqual({"2536", "8744"}, audit.extract_case_emsgs(body))

    def test_maps_named_case_labels_to_emsgs(self):
        body = "case GBE_kDotaFindTopSourceTVGames:\ncase GBE_kDotaCustomGameInfoRequest:\nbreak;"
        self.assertEqual({"8009", "8020"}, audit.extract_case_emsgs(body))


class ExtractRequestEmsgComparisonsHelperTest(unittest.TestCase):
    def test_extracts_numeric_request_comparisons(self):
        body = "request_emsg == 8744 || request_emsg == 7777"
        self.assertEqual({"8744", "7777"}, audit.extract_request_emsg_comparisons(body))

    def test_strips_unsigned_suffix_from_numeric_request_comparisons(self):
        body = "request_emsg == 8744u"
        self.assertEqual({"8744"}, audit.extract_request_emsg_comparisons(body))

    def test_maps_named_request_tokens_to_emsgs(self):
        body = "request_emsg == GBE_kSteamGamesPlayedWithDataBlob || request_emsg == GBE_kSteamAuthList"
        self.assertEqual({"5410", "5432"}, audit.extract_request_emsg_comparisons(body))


class ExtractAdapterHandlerCallsHelperTest(unittest.TestCase):
    def test_extracts_single_adapter_handler_call(self):
        body = "return self->GBE_HandleDotaJoinChatRequest(ctx);"
        self.assertEqual(["GBE_HandleDotaJoinChatRequest"], audit.extract_adapter_handler_calls(body))

    def test_extracts_multiple_adapter_handler_calls_in_order(self):
        body = "self->GBE_HandleDotaOneRequest(ctx); self->GBE_HandleDotaTwoRequest(ctx);"
        self.assertEqual(
            ["GBE_HandleDotaOneRequest", "GBE_HandleDotaTwoRequest"],
            audit.extract_adapter_handler_calls(body),
        )

    def test_ignores_non_request_handler_names(self):
        body = "return self->GBE_HandleDotaJoinChat(ctx);"
        self.assertEqual([], audit.extract_adapter_handler_calls(body))

    def test_returns_empty_list_when_no_handler_call_exists(self):
        self.assertEqual([], audit.extract_adapter_handler_calls("return false;"))


class ExtractReplayFixtureLabelsHelperTest(unittest.TestCase):
    def test_extracts_third_field_labels(self):
        text = "100 direct join_chat\n200 wrapped lobby_create\n"
        self.assertEqual({"join_chat", "lobby_create"}, audit.extract_replay_fixture_labels(text))

    def test_ignores_blank_and_malformed_lines(self):
        text = "\n100 direct\nmalformed\n200 wrapped label\n"
        self.assertEqual({"label"}, audit.extract_replay_fixture_labels(text))

    def test_preserves_label_text_after_second_field(self):
        text = "100 direct label with spaces\n"
        self.assertEqual({"label with spaces"}, audit.extract_replay_fixture_labels(text))


class SortedNumericValuesHelperTest(unittest.TestCase):
    def test_sorts_string_values_numerically(self):
        self.assertEqual(["3", "20", "100"], audit.sorted_numeric_values({"20", "100", "3"}))


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


class PublicCapabilityHeaderAuditTest(unittest.TestCase):
    def test_includes_all_shared_capability_headers(self):
        header_names = {header.rsplit("/", 1)[-1] for header in audit.PUBLIC_HEADERS}
        self.assertTrue({
            "gbe_dota_gc_diagnostics.h",
            "gbe_dota_lobby_handler_helpers.h",
            "gbe_dota_locator.h",
            "gbe_dota_reconnect_network.h",
            "gbe_dota_vpk_loot_cache.h",
            "gbe_dota_server_hello_cache.h",
        }.issubset(header_names))

    def test_capability_headers_declare_their_shared_functions(self):
        expected_symbols = {
            "gbe_dota_gc_diagnostics.h": {
                "GBE_GC_DebugLog",
                "GBE_DescribeDotaLaunchPhase",
                "GBE_LogDotaSOCacheSubscribedSummary",
                "GBE_LogGCProtoBoundary",
                "GBE_LogDotaResponsePacket",
            },
            "gbe_dota_vpk_loot_cache.h": {
                "GBE_GetDotaVpkLootData",
                "GBE_SetDotaVpkLootData",
            },
            "gbe_dota_lobby_handler_helpers.h": {
                "GBE_ApplyDotaCustomGameDetailsRequest",
                "GBE_NormalizeDotaCustomGameDetailsFromInstalledMod",
                "GBE_GenerateDotaLobbyId",
                "GBE_GenerateDotaMatchId",
                "GBE_AdaptDotaLobbyInviteCacheSubscribedPayload",
                "GBE_IsDotaLobbyInviteCacheSubscribedPayload",
            },
            "gbe_dota_locator.h": {
                "GBE_GetSharedDotaLobbyStateStore",
                "GBE_BindSharedDotaLobbyStateStore",
                "GBE_UnbindSharedDotaLobbyStateStore",
                "GBE_BindDotaRuntimeState",
                "GBE_UnbindDotaRuntimeState",
                "GBE_DotaRuntimeState",
            },
            "gbe_dota_reconnect_network.h": {
                "GBE_PrepareDotaReconnectPostConnectionState",
                "GBE_ExecuteDotaReconnectPostEffects",
                "GBE_ExecuteDotaReconnectPostConnectionState",
            },
            "gbe_dota_server_hello_cache.h": {
                "GBE_HasLastDotaServerHelloContext",
                "GBE_GetLastDotaServerHelloContext",
                "GBE_SetLastDotaServerHelloContext",
                "GBE_ClearLastDotaServerHelloContext",
            },
        }
        for header in audit.PUBLIC_HEADERS:
            header_name = header.rsplit("/", 1)[-1]
            if header_name in expected_symbols:
                self.assertEqual(
                    expected_symbols[header_name],
                    audit.extract_header_symbols(audit.read(header)),
                )


class PayloadSnapshotProjectionAuditTest(unittest.TestCase):
    def test_requires_all_payload_paths_to_use_pure_projection_facade(self):
        self.assertEqual([], audit.audit_payload_snapshot_projection())

    def test_requires_explicit_generic_metadata_capture_modes(self):
        self.assertEqual([], audit.audit_generic_metadata_capture_modes())


class RegistryDefensiveTemplateRoutingAuditTest(unittest.TestCase):
    def test_accepts_centralized_registry_defensive_template_routing(self):
        self.assertEqual([], audit.audit_registry_defensive_template_routing())

    def test_rejects_missing_registry_defensive_helper_case(self):
        handler_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_template_replay_handlers.cpp"))
        handler_text = handler_text.replace("        case 7091:\n", "")
        issues = audit.audit_registry_defensive_template_routing(handler_text=handler_text)
        self.assertTrue(any("helper emsgs" in issue for issue in issues))

    def test_rejects_registry_defensive_case_in_template_switch(self):
        handler_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_template_replay_handlers.cpp"))
        handler_text = handler_text.replace(
            "        // TEMPLATE_ONLY (synthetic custom game info)\n",
            "        case 8879:\n            return GBE_HandleDotaRankRequest(body, body_size, has_source_job, source_job);\n        // TEMPLATE_ONLY (synthetic custom game info)\n",
        )
        self.assertIn(
            "template replay: registry-defensive emsg 8879 remains in template-only switch",
            audit.audit_registry_defensive_template_routing(handler_text=handler_text),
        )

    def test_rejects_duplicate_registry_defensive_inventory_row(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| 7091 | REGISTRY_DEFENSIVE | → WatchGame handler |"
        inventory_text = inventory_text.replace(row, f"{row}\n{row}")
        self.assertIn(
            "MESSAGE_ROUTING registry-defensive inventory duplicates 7091",
            audit.audit_registry_defensive_template_routing(inventory_text=inventory_text),
        )

    def test_rejects_missing_template_replay_inventory_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("## 4. template_replay", "## 4. moved_template_replay")
        self.assertIn(
            "MESSAGE_ROUTING registry-defensive inventory missing template_replay section",
            audit.audit_registry_defensive_template_routing(inventory_text=inventory_text),
        )

    def test_ignores_registry_defensive_row_after_template_replay_section_when_separator_is_missing(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("\n---\n\n## 5. 解析层职责", "\n\n## 5. 解析层职责", 1)
        inventory_text += "\n| 7091 | REGISTRY_DEFENSIVE | outside template section |\n"
        self.assertEqual([], audit.audit_registry_defensive_template_routing(inventory_text=inventory_text))


class TemplateOnlyInventoryGuardAuditTest(unittest.TestCase):
    def test_accepts_template_only_switch_and_inventory_alignment(self):
        self.assertEqual([], audit.audit_template_only_inventory_guard())

    def test_rejects_untracked_template_only_switch_case(self):
        handler_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_template_replay_handlers.cpp"))
        handler_text = handler_text.replace(
            "        // TEMPLATE_ONLY (canned)\n        case 2536:\n",
            "        // TEMPLATE_ONLY (canned)\n        case 9999:\n        case 2536:\n",
        )
        issues = audit.audit_template_only_inventory_guard(handler_text=handler_text)
        self.assertTrue(any("template-only switch emsgs" in issue for issue in issues))

    def test_rejects_template_only_inventory_drift(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("| 8218 | TEMPLATE_ONLY | synthetic tip success |", "")
        issues = audit.audit_template_only_inventory_guard(inventory_text=inventory_text)
        self.assertTrue(any("MESSAGE_ROUTING template-only emsgs" in issue for issue in issues))

    def test_rejects_duplicate_template_only_inventory_emsg(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| 8218 | TEMPLATE_ONLY | synthetic tip success |"
        inventory_text = inventory_text.replace(row, f"{row}\n{row}")
        self.assertIn(
            "MESSAGE_ROUTING template-only inventory duplicates 8218",
            audit.audit_template_only_inventory_guard(inventory_text=inventory_text),
        )

    def test_rejects_template_only_row_outside_template_replay_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| 8218 | TEMPLATE_ONLY | synthetic tip success |"
        inventory_text = inventory_text.replace(row, "")
        inventory_text += f"\n\n{row}\n"
        issues = audit.audit_template_only_inventory_guard(inventory_text=inventory_text)
        self.assertTrue(any("MESSAGE_ROUTING template-only emsgs" in issue for issue in issues))

    def test_rejects_missing_template_replay_inventory_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("## 4. template_replay", "## 4. moved_template_replay")
        self.assertIn(
            "MESSAGE_ROUTING template-only inventory missing template_replay section",
            audit.audit_template_only_inventory_guard(inventory_text=inventory_text),
        )

    def test_ignores_template_only_row_after_template_replay_section_when_separator_is_missing(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("\n---\n\n## 5. 解析层职责", "\n\n## 5. 解析层职责", 1)
        inventory_text += "\n| 9999 | TEMPLATE_ONLY | outside template section |\n"
        self.assertEqual([], audit.audit_template_only_inventory_guard(inventory_text=inventory_text))


class DirectConditionalFallbackRoutingAuditTest(unittest.TestCase):
    def test_accepts_centralized_direct_conditional_fallback_routing(self):
        self.assertEqual([], audit.audit_direct_conditional_fallback_routing())

    def test_rejects_missing_direct_conditional_helper_case(self):
        handler_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_post_login_handlers.cpp"))
        handler_text = handler_text.replace("    if (request_emsg == GBE_kSteamAuthList && track_late_steam_chain) {", "    if (false && track_late_steam_chain) {")
        issues = audit.audit_direct_conditional_fallback_routing(handler_text=handler_text)
        self.assertTrue(any("helper emsgs" in issue for issue in issues))

    def test_rejects_inline_direct_conditional_check(self):
        handler_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_post_login_handlers.cpp"))
        handler_text = handler_text.replace(
            "    const GBE_DotaDirectConditionalFallbackResult conditional_fallback =\n",
            "    if (request_emsg == 8744u)\n        GBE_GC_DebugLog(\"GC_DOTA_DIRECT\", \"probe\");\n\n    const GBE_DotaDirectConditionalFallbackResult conditional_fallback =\n",
        )
        self.assertIn(
            "direct post-login: inline conditional fallback check remains for 8744u",
            audit.audit_direct_conditional_fallback_routing(handler_text=handler_text),
        )

    def test_rejects_inventory_drift(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("| AuthList (5432) | CONDITIONAL_CONSUME | 同上 | 同上 |", "")
        issues = audit.audit_direct_conditional_fallback_routing(inventory_text=inventory_text)
        self.assertTrue(any("MESSAGE_ROUTING direct conditional fallback emsgs" in issue for issue in issues))

    def test_rejects_duplicate_direct_conditional_inventory_row(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| AuthList (5432) | CONDITIONAL_CONSUME | 同上 | 同上 |"
        inventory_text = inventory_text.replace(row, f"{row}\n{row}")
        self.assertIn(
            "MESSAGE_ROUTING direct conditional fallback inventory duplicates 5432",
            audit.audit_direct_conditional_fallback_routing(inventory_text=inventory_text),
        )

    def test_rejects_missing_fallback_inventory_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("## 2. 仍留在 if/fallback 的路径", "## 2. moved_fallback")
        self.assertIn(
            "MESSAGE_ROUTING direct conditional fallback inventory missing fallback section",
            audit.audit_direct_conditional_fallback_routing(inventory_text=inventory_text),
        )

    def test_rejects_direct_conditional_row_after_fallback_section_when_separator_is_missing(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| AuthList (5432) | CONDITIONAL_CONSUME | 同上 | 同上 |"
        inventory_text = inventory_text.replace(row, "")
        inventory_text = inventory_text.replace("\n---\n\n## 3. Hello / Welcome pipeline", "\n\n## 3. Hello / Welcome pipeline", 1)
        inventory_text += f"\n{row}\n"
        issues = audit.audit_direct_conditional_fallback_routing(inventory_text=inventory_text)
        self.assertTrue(any("MESSAGE_ROUTING direct conditional fallback emsgs" in issue for issue in issues))


class WrappedHardMissRoutingAuditTest(unittest.TestCase):
    def test_accepts_centralized_wrapped_hard_miss_routing(self):
        self.assertEqual([], audit.audit_wrapped_hard_miss_routing())

    def test_rejects_missing_wrapped_hard_miss_helper(self):
        handler_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_post_login_handlers.cpp"))
        handler_text = handler_text.replace("GBE_HandleDotaWrappedHardMiss", "GBE_HandleDotaWrappedMissInline")
        issues = audit.audit_wrapped_hard_miss_routing(handler_text=handler_text)
        self.assertTrue(any("missing explicit hard-miss helper" in issue for issue in issues))

    def test_rejects_wrapped_miss_template_replay(self):
        handler_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_post_login_handlers.cpp"))
        handler_text = handler_text.replace(
            "    return GBE_HandleDotaWrappedHardMiss(route_context);",
            "    return GBE_HandleDotaTemplateReplayRequest(route_context.inner_emsg, reinterpret_cast<const uint8 *>(route_context.body.data()), route_context.body.size(), route_context.has_request_job, route_context.request_job_id);",
        )
        issues = audit.audit_wrapped_hard_miss_routing(handler_text=handler_text)
        self.assertTrue(any("must not route miss to template replay" in issue for issue in issues))

    def test_rejects_wrapped_hard_miss_inventory_drift(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("经 wrapped hard miss helper ", "")
        issues = audit.audit_wrapped_hard_miss_routing(inventory_text=inventory_text)
        self.assertTrue(any("wrapped hard miss must mention wrapped hard miss helper" in issue for issue in issues))

    def test_rejects_wrapped_hard_miss_marker_outside_fallback_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| （未注册 miss） | HARD_MISS | 经 wrapped hard miss helper log + return false | B4：删除误落到 SetTeamSlot 的 dead fallback |"
        inventory_text = inventory_text.replace(row, "")
        inventory_text += "\n\n| moved | HARD_MISS | wrapped hard miss helper | outside fallback |\n"
        self.assertIn(
            "MESSAGE_ROUTING wrapped hard miss must mention wrapped hard miss helper",
            audit.audit_wrapped_hard_miss_routing(inventory_text=inventory_text),
        )

    def test_rejects_missing_fallback_inventory_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("## 2. 仍留在 if/fallback 的路径", "## 2. moved_fallback")
        self.assertIn(
            "MESSAGE_ROUTING wrapped hard miss inventory missing fallback section",
            audit.audit_wrapped_hard_miss_routing(inventory_text=inventory_text),
        )

    def test_rejects_wrapped_hard_miss_marker_after_fallback_section_when_separator_is_missing(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| （未注册 miss） | HARD_MISS | 经 wrapped hard miss helper log + return false | B4：删除误落到 SetTeamSlot 的 dead fallback |"
        inventory_text = inventory_text.replace(row, "")
        inventory_text = inventory_text.replace("\n---\n\n## 3. Hello / Welcome pipeline", "\n\n## 3. Hello / Welcome pipeline", 1)
        inventory_text += "\n| moved | HARD_MISS | wrapped hard miss helper | outside fallback |\n"
        self.assertIn(
            "MESSAGE_ROUTING wrapped hard miss must mention wrapped hard miss helper",
            audit.audit_wrapped_hard_miss_routing(inventory_text=inventory_text),
        )


class LegacyWrappedParserGuardAuditTest(unittest.TestCase):
    def test_accepts_legacy_wrapped_parser_without_production_calls(self):
        self.assertEqual([], audit.audit_legacy_wrapped_parser_guard())

    def test_rejects_production_call_to_legacy_wrapped_parser(self):
        issues = audit.audit_legacy_wrapped_parser_guard(
            source_texts={
                "gbe_dota_post_login_handlers.cpp": "GBE_ExtractWrappedDotaDirectContext(pubData, cubData, context);",
            }
        )
        self.assertIn(
            "gbe_dota_post_login_handlers.cpp:1: production must not call legacy wrapped parser GBE_ExtractWrappedDotaDirectContext",
            issues,
        )

    def test_rejects_missing_legacy_unused_inventory_marker(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("**LEGACY_UNUSED**", "ACTIVE")
        issues = audit.audit_legacy_wrapped_parser_guard(inventory_text=inventory_text)
        self.assertIn(
            "MESSAGE_ROUTING must document GBE_ExtractWrappedDotaDirectContext as LEGACY_UNUSED",
            issues,
        )

    def test_rejects_legacy_unused_marker_outside_parser_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("**LEGACY_UNUSED**", "ACTIVE")
        inventory_text += "\n\n`GBE_ExtractWrappedDotaDirectContext` **LEGACY_UNUSED** outside parser section\n"
        self.assertIn(
            "MESSAGE_ROUTING must document GBE_ExtractWrappedDotaDirectContext as LEGACY_UNUSED",
            audit.audit_legacy_wrapped_parser_guard(inventory_text=inventory_text),
        )

    def test_rejects_missing_parser_inventory_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("## 5. 解析层职责", "## 5. moved_parser")
        self.assertIn(
            "MESSAGE_ROUTING legacy wrapped parser inventory missing parser section",
            audit.audit_legacy_wrapped_parser_guard(inventory_text=inventory_text),
        )

    def test_rejects_legacy_unused_marker_after_parser_section_when_separator_is_missing(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("**LEGACY_UNUSED**", "ACTIVE")
        inventory_text = inventory_text.replace("\n---\n\n## 6. 维护检查清单", "\n\n## 6. 维护检查清单", 1)
        inventory_text += "\n`GBE_ExtractWrappedDotaDirectContext` **LEGACY_UNUSED** outside parser section\n"
        self.assertIn(
            "MESSAGE_ROUTING must document GBE_ExtractWrappedDotaDirectContext as LEGACY_UNUSED",
            audit.audit_legacy_wrapped_parser_guard(inventory_text=inventory_text),
        )


class PublicHeaderDefinitionAuditTest(unittest.TestCase):
    def test_ignores_virtual_member_destructor(self):
        self.assertEqual(
            set(),
            audit.extract_header_symbols("struct GBE_Interface { virtual ~GBE_Interface() = default; };"),
        )

    def test_accepts_declarations_with_definitions(self):
        declared, zombies = audit.audit_public_header_definitions(
            ["void GBE_Alpha();", "bool GBE_Beta(int value);"],
            {"GBE_Alpha": ("alpha.cpp", 1, "free_function"), "GBE_Beta": ("beta.cpp", 1, "free_function")},
        )
        self.assertEqual({"GBE_Alpha", "GBE_Beta"}, declared)
        self.assertEqual([], zombies)

    def test_reports_declaration_without_definition(self):
        declared, zombies = audit.audit_public_header_definitions(
            ["void GBE_Alpha();", "bool GBE_Beta(int value);"],
            {"GBE_Alpha": ("alpha.cpp", 1, "free_function")},
        )
        self.assertEqual({"GBE_Alpha", "GBE_Beta"}, declared)
        self.assertEqual(["GBE_Beta"], zombies)

    def test_accepts_inline_header_definition(self):
        declared, zombies = audit.audit_public_header_definitions(
            ["inline bool GBE_Alpha() { return true; }"],
            {},
        )
        self.assertEqual({"GBE_Alpha"}, declared)
        self.assertEqual([], zombies)


class LifecycleTransitionGateAuditTest(unittest.TestCase):
    def valid_sources(self):
        return {
            base: "\n".join(required_tokens)
            for base, required_tokens in audit.LIFECYCLE_TRANSITION_GATES.items()
        }

    def test_accepts_all_migrated_transition_and_effect_gates(self):
        self.assertEqual([], audit.audit_lifecycle_transition_gates(self.valid_sources()))

    def test_rejects_missing_transition_gate(self):
        sources = self.valid_sources()
        sources["gbe_dota_match_handlers.cpp"] = sources["gbe_dota_match_handlers.cpp"].replace(
            "transition_runtime_poll(",
            "",
        )
        self.assertIn(
            "gbe_dota_match_handlers.cpp: lifecycle path is missing transition gate token transition_runtime_poll(",
            audit.audit_lifecycle_transition_gates(sources),
        )

    def test_rejects_missing_typed_effect_gate(self):
        sources = self.valid_sources()
        sources["gbe_dota_lobby_lifecycle_handlers.cpp"] = sources["gbe_dota_lobby_lifecycle_handlers.cpp"].replace(
            "EffectKind::TeardownAbandonInitiateRequested",
            "",
        )
        self.assertIn(
            "gbe_dota_lobby_lifecycle_handlers.cpp: lifecycle path is missing transition gate token EffectKind::TeardownAbandonInitiateRequested",
            audit.audit_lifecycle_transition_gates(sources),
        )


class CoreStateMachineBoundaryAuditTest(unittest.TestCase):
    LIFECYCLE_HEADER = "\n".join(audit.CORE_STATE_MACHINE_HEADER_TOKENS)
    GENERATION_HEADER = "constexpr AdvanceResult advance(Boundary boundary)"

    def valid_sources(self):
        return {
            audit.GENERATION_COUNTER_OWNER: "GBE_dota_lobby_generation_counter.advance(boundary);",
            audit.GENERATION_COUNTER_SYNC_OWNER: "\n".join(
                "GBE_dota_lobby_generation_counter = gbe::dota_lobby_generation::Counter(initial);"
                for _ in range(audit.GENERATION_COUNTER_SYNC_BASELINE)
            ),
            audit.RECONNECT_TRANSITION_OWNER: """
GBE_PrepareDotaReconnectPostConnectionState(
GBE_ExecuteDotaReconnectPostEffects(
connection_state.begin_generation(generation);
connection_state.should_connect_direct(server_id, endpoint);
connection_state.record_direct_connect(server_id, endpoint);
connection_state.record_engine_callback(server_id, endpoint);
""",
        }

    def audit(self, sources=None, lifecycle_header=None):
        return audit.audit_core_state_machine_boundaries(
            lifecycle_header or self.LIFECYCLE_HEADER,
            self.GENERATION_HEADER,
            sources or self.valid_sources(),
        )

    def test_accepts_canonical_core_transition_owners(self):
        self.assertEqual([], self.audit())

    def test_rejects_missing_pure_lifecycle_transition(self):
        header = self.LIFECYCLE_HEADER.replace("constexpr MachineTransitionResult transition_teardown(", "")
        self.assertIn(
            "gbe_dota_lifecycle_state_machine.h: missing pure core transition constexpr MachineTransitionResult transition_teardown(",
            self.audit(lifecycle_header=header),
        )

    def test_rejects_generation_advance_bypass(self):
        sources = self.valid_sources()
        sources["gbe_dota_lobby_lifecycle_handlers.cpp"] = "GBE_dota_lobby_generation_counter.advance(boundary);"
        self.assertIn(
            "generation counter advance owners changed: expected [steam_game_coordinator.cpp], got ['gbe_dota_lobby_lifecycle_handlers.cpp', 'steam_game_coordinator.cpp']",
            self.audit(sources),
        )

    def test_rejects_reconnect_transition_bypass(self):
        sources = self.valid_sources()
        sources["steam_networking_socketsserialized.cpp"] = "connection_state.begin_generation(generation);"
        self.assertIn(
            "reconnect generation/dedup transition owners changed: expected [gbe_dota_reconnect_network.cpp], got ['gbe_dota_reconnect_network.cpp', 'steam_networking_socketsserialized.cpp']",
            self.audit(sources),
        )


class AsyncGenerationSafetyAuditTest(unittest.TestCase):
    def valid_sources(self):
        return {
            filename: "\n".join(tokens)
            for filename, tokens in audit.ASYNC_GENERATION_GATES.items()
        }

    def valid_tests(self):
        return "\n".join(
            f"void {name}();\n{name}();"
            for name in audit.ASYNC_GENERATION_REGRESSION_TESTS
        )

    def test_accepts_all_async_generation_gates(self):
        self.assertEqual([], audit.audit_async_generation_safety(self.valid_sources(), self.valid_tests()))

    def test_rejects_delayed_message_without_final_validation(self):
        sources = self.valid_sources()
        sources["steam_game_coordinator.cpp"] = sources["steam_game_coordinator.cpp"].replace(
            'GBE_IsQueuedLobbyMessageCurrent(*it, "before_incoming_queue")',
            "",
        )
        self.assertIn(
            'steam_game_coordinator.cpp: async generation boundary is missing GBE_IsQueuedLobbyMessageCurrent(*it, "before_incoming_queue")',
            audit.audit_async_generation_safety(sources, self.valid_tests()),
        )

    def test_rejects_reconnect_callback_without_generation_guard(self):
        sources = self.valid_sources()
        sources["gbe_dota_reconnect_network_adapter.cpp"] = sources["gbe_dota_reconnect_network_adapter.cpp"].replace(
            "current_context.generation == expected_generation",
            "",
        )
        self.assertIn(
            "gbe_dota_reconnect_network_adapter.cpp: async generation boundary is missing current_context.generation == expected_generation",
            audit.audit_async_generation_safety(sources, self.valid_tests()),
        )

    def test_rejects_missing_executed_stale_work_regression(self):
        tests = self.valid_tests().replace("test_lobby_stale_postgame_task_is_rejected();", "")
        self.assertIn(
            "gbe_dota_handler_test/smoke_test.cpp: missing executed async generation regression test_lobby_stale_postgame_task_is_rejected",
            audit.audit_async_generation_safety(self.valid_sources(), tests),
        )


class TestCredibilityAuditTest(unittest.TestCase):
    def valid_sources(self):
        return {
            path: "\n".join(f"void {name}();\n{name}();" for name in names)
            for path, names in audit.TEST_CREDIBILITY_GATES.items()
        }

    def valid_runner(self):
        return "\n".join(f"build {target}\nrun {target}" for target in audit.TEST_CREDIBILITY_RUNNER_TARGETS)

    def test_accepts_executed_layered_high_risk_tests(self):
        self.assertEqual([], audit.audit_test_credibility(self.valid_sources(), self.valid_runner()))

    def test_rejects_property_test_defined_without_execution(self):
        sources = self.valid_sources()
        path = "tools/gbe_dota_lifecycle_state_machine_test/gbe_dota_lifecycle_state_machine_test.cpp"
        sources[path] = sources[path].replace("run_state_machine_properties();", "")
        self.assertIn(
            f"{path}: high-risk test is not defined and executed: run_state_machine_properties",
            audit.audit_test_credibility(sources, self.valid_runner()),
        )

    def test_rejects_missing_executor_coverage(self):
        sources = self.valid_sources()
        path = "tools/gbe_dota_handler_test/smoke_test.cpp"
        sources[path] = sources[path].replace("test_lifecycle_executor_push_routes_and_failure_policy();", "")
        self.assertIn(
            f"{path}: high-risk test is not defined and executed: test_lifecycle_executor_push_routes_and_failure_policy",
            audit.audit_test_credibility(sources, self.valid_runner()),
        )

    def test_rejects_test_target_missing_from_runner(self):
        runner = self.valid_runner().replace("run gbe_dota_reconnect_network_test", "")
        self.assertIn(
            "tools/run_gc_offline_tests.sh: high-risk test target is not built and run: gbe_dota_reconnect_network_test",
            audit.audit_test_credibility(self.valid_sources(), runner),
        )


class ArchitectureInvestmentInputsAuditTest(unittest.TestCase):
    VALID = {
        "schema_version": 1,
        "decision_date": "2026-07-11",
        "recheck_triggers": ["major_concurrency_expansion", "major_lifecycle_state_machine_expansion"],
        "concurrency": {
            "tsan_race_reports": 0,
            "lock_order_depth": 2,
            "out_of_order_mutation_defects_in_phase": 0,
            "unowned_cross_thread_business_writers": 0,
            "actor_gate": "closed",
        },
        "state_machine": {
            "state_count": 8,
            "event_count": 13,
            "state_event_pairs": 104,
            "length_five_sequences_checked": 100000,
            "differential_steps_checked": 4096,
            "escaped_ordering_defects_in_phase": 0,
            "active_transition_maintainers_in_release": 1,
            "critical_irreversible_failure_scope": False,
            "formal_model_gate": "closed",
        },
        "model_consistency": {
            "maintained_formal_model": False,
            "named_owner_and_reviewer": False,
            "versioned_vector_schema": False,
            "pinned_checker": False,
            "model_ci_gate": "closed",
        },
        "ci_seconds": {
            "local_fast_offline": 135.14,
            "local_clang_tsan": 59.48,
            "model_checker_limit": 600,
        },
    }

    def audit(self, value):
        return audit.audit_architecture_investment_inputs(json.dumps(value))

    def test_accepts_complete_repeatable_inputs(self):
        self.assertEqual([], self.audit(self.VALID))

    def test_rejects_missing_recheck_trigger(self):
        value = json.loads(json.dumps(self.VALID))
        value["recheck_triggers"].remove("major_concurrency_expansion")
        self.assertIn(
            "architecture-investment-inputs.json: both major expansion recheck triggers are required",
            self.audit(value),
        )

    def test_rejects_state_space_count_drift(self):
        value = json.loads(json.dumps(self.VALID))
        value["state_machine"]["state_event_pairs"] = 103
        self.assertIn(
            "architecture-investment-inputs.json: state_event_pairs must equal state_count * event_count",
            self.audit(value),
        )

    def test_rejects_missing_ci_runtime(self):
        value = json.loads(json.dumps(self.VALID))
        value["ci_seconds"]["local_clang_tsan"] = 0
        self.assertIn(
            "architecture-investment-inputs.json: ci_seconds.local_clang_tsan must be positive",
            self.audit(value),
        )


class ArchitectureInvestmentBoundaryAuditTest(unittest.TestCase):
    def closed_records(self):
        value = json.loads(json.dumps(ArchitectureInvestmentInputsAuditTest.VALID))
        gates = """
The Actor gate is closed.
The formal model gate is closed.
The Model Consistency CI Gate is therefore closed.
"""
        tasklist = "Actor 门禁关闭\n形式化模型门禁关闭\nP17 门禁关闭"
        return value, gates, tasklist

    def test_accepts_evidence_derived_closed_gates(self):
        value, gates, tasklist = self.closed_records()
        self.assertEqual(
            [],
            audit.audit_architecture_investment_boundaries(json.dumps(value), gates, tasklist),
        )

    def test_rejects_actor_decision_inconsistent_with_race_evidence(self):
        value, gates, tasklist = self.closed_records()
        value["concurrency"]["tsan_race_reports"] = 1
        self.assertIn(
            "architecture-investment-inputs.json: actor_gate must be open for the recorded evidence",
            audit.audit_architecture_investment_boundaries(json.dumps(value), gates, tasklist),
        )

    def test_rejects_formal_model_decision_inconsistent_with_thresholds(self):
        value, gates, tasklist = self.closed_records()
        value["state_machine"]["state_count"] = 17
        value["state_machine"]["state_event_pairs"] = 221
        value["state_machine"]["active_transition_maintainers_in_release"] = 3
        self.assertIn(
            "architecture-investment-inputs.json: formal_model_gate must be open for the recorded evidence",
            audit.audit_architecture_investment_boundaries(json.dumps(value), gates, tasklist),
        )

    def test_rejects_documented_decision_drift(self):
        value, gates, tasklist = self.closed_records()
        gates = gates.replace("The formal model gate is closed.\n", "")
        self.assertIn(
            "architecture-investment-gates.md: missing derived decision 'The formal model gate is closed.'",
            audit.audit_architecture_investment_boundaries(json.dumps(value), gates, tasklist),
        )


class HandlerResponsibilityBoundaryAuditTest(unittest.TestCase):
    def test_accepts_existing_or_reduced_compatibility_operations(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "push_incoming_now(24, payload);",
            "gbe_dota_inventory_handlers.cpp": "network->sendToAllGameservers(&message, true);",
        }
        self.assertEqual([], audit.audit_handler_responsibility_boundaries(sources)[0])

    def test_rejects_new_direct_response_push(self):
        sources = {
            "gbe_dota_match_handlers.cpp": "push_incoming_now(1, a); push_incoming_now(2, b);",
        }
        self.assertIn(
            "gbe_dota_match_handlers.cpp: direct push_incoming_now count 2 exceeds accepted handler boundary 1; route new work through a coordinator or executor",
            audit.audit_handler_responsibility_boundaries(sources)[0],
        )

    def test_rejects_handler_store_access(self):
        sources = {
            "gbe_dota_match_handlers.cpp": "auto snapshot = GBE_GetSharedDotaLobbyStateStore().snapshot();",
        }
        self.assertIn(
            "gbe_dota_match_handlers.cpp: direct shared Store accessor count 1 exceeds accepted handler boundary 0; route new work through a coordinator or executor",
            audit.audit_handler_responsibility_boundaries(sources)[0],
        )

    def test_ignores_mentions_inside_comments(self):
        sources = {
            "gbe_dota_match_handlers.cpp": "// push_incoming_now(1, payload);\n/* GBE_local_lobby = lobby; */",
        }
        self.assertEqual([], audit.audit_handler_responsibility_boundaries(sources)[0])


class DirectLocalLobbyWriteAuditTest(unittest.TestCase):
    def test_accepts_helper_wrapped_local_writes_and_comments(self):
        sources = {
            "gbe_dota_lobby_state.cpp": "lobby = plan.lobby; lobby.state = 1u;",
            "gbe_dota_lobby_create_handlers.cpp": "// GBE_local_lobby = lobby;\napply_create_lobby_state_plan(GBE_local_lobby, plan);",
        }
        self.assertEqual([], audit.audit_direct_local_lobby_writes(sources))

    def test_accepts_const_alias_to_local_lobby(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "const auto& lobby = GBE_local_lobby; return lobby.active;",
        }
        self.assertEqual([], audit.audit_direct_local_lobby_writes(sources))

    def test_rejects_direct_object_write(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "GBE_local_lobby = plan.lobby;",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: direct GBE_local_lobby object write count 1; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_field_write(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "GBE_local_lobby.state = 1u;",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: direct GBE_local_lobby field write count 1; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_nested_field_write(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "GBE_local_lobby.custom_game.game_id = 1ull;",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: direct GBE_local_lobby field write count 1; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_compound_and_increment_field_writes(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "GBE_local_lobby.generation += 1u; GBE_local_lobby.state++;",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: direct GBE_local_lobby field write count 2; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_prefix_increment_field_write(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "++GBE_local_lobby.generation;",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: direct GBE_local_lobby field write count 1; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_mutating_method_field_write(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "GBE_local_lobby.members.clear();",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: direct GBE_local_lobby field write count 1; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_target_field_write(self):
        sources = {
            "gbe_dota_custom_game_lifecycle_coordinator.cpp": "options.client_target->GBE_local_lobby.state = 1u;",
        }
        self.assertIn(
            "gbe_dota_custom_game_lifecycle_coordinator.cpp: direct GBE_local_lobby field write count 1; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_target_nested_compound_and_method_field_writes(self):
        sources = {
            "gbe_dota_custom_game_lifecycle_coordinator.cpp": (
                "options.client_target->GBE_local_lobby.custom_game.game_id = 1ull; "
                "options.client_target->GBE_local_lobby.generation += 1u; "
                "options.client_target->GBE_local_lobby.members.clear();"
            ),
        }
        self.assertIn(
            "gbe_dota_custom_game_lifecycle_coordinator.cpp: direct GBE_local_lobby field write count 3; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_direct_target_object_write(self):
        sources = {
            "gbe_dota_custom_game_lifecycle_coordinator.cpp": "options.client_target->GBE_local_lobby = snapshot;",
        }
        self.assertIn(
            "gbe_dota_custom_game_lifecycle_coordinator.cpp: direct GBE_local_lobby object write count 1; route through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_mutable_alias_to_local_lobby(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "auto& lobby = GBE_local_lobby; lobby.state = 1u;",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: mutable GBE_local_lobby alias count 1; route writes through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )

    def test_rejects_mutable_pointer_alias_to_local_lobby(self):
        sources = {
            "gbe_dota_lobby_create_handlers.cpp": "GBE_DotaLobbyState* lobby = &GBE_local_lobby; lobby->state = 1u;",
        }
        self.assertIn(
            "gbe_dota_lobby_create_handlers.cpp: mutable GBE_local_lobby alias count 1; route writes through a named state helper",
            audit.audit_direct_local_lobby_writes(sources),
        )


class StateEffectOwnershipAuditTest(unittest.TestCase):
    def valid_sources(self):
        sources = {base: "return plan;" for base in audit.PURE_DOTA_PLANNER_FILES}
        sources[audit.LIFECYCLE_EXECUTOR_OWNER] = "\n".join(
            f"{api}();" for api in audit.LIFECYCLE_SIDE_EFFECT_APIS
        )
        sources["gbe_dota_reconnect_network_adapter.cpp"] = "direct_sockets->ConnectByIPAddress(address);"
        return sources

    def test_accepts_canonical_planner_executor_and_network_owners(self):
        self.assertEqual([], audit.audit_state_effect_ownership(self.valid_sources()))

    def test_rejects_side_effect_inside_pure_planner(self):
        sources = self.valid_sources()
        sources["gbe_dota_lobby_flow.cpp"] = "push_incoming_now(24, payload);"
        self.assertIn(
            "gbe_dota_lobby_flow.cpp: pure planner directly uses side-effect or shared-state token push_incoming_now(",
            audit.audit_state_effect_ownership(sources),
        )

    def test_rejects_missing_executor_owned_effect(self):
        sources = self.valid_sources()
        sources[audit.LIFECYCLE_EXECUTOR_OWNER] = sources[audit.LIFECYCLE_EXECUTOR_OWNER].replace(
            "GBE_SendDotaPracticeLobbyDetailsUpdate();",
            "",
        )
        self.assertIn(
            f"{audit.LIFECYCLE_EXECUTOR_OWNER}: lifecycle executor no longer owns GBE_SendDotaPracticeLobbyDetailsUpdate",
            audit.audit_state_effect_ownership(sources),
        )

    def test_rejects_reconnect_network_call_outside_adapter(self):
        sources = self.valid_sources()
        sources["gbe_dota_lobby_flow.cpp"] = "socket.ConnectByIPAddress(address);"
        self.assertIn(
            "gbe_dota_lobby_flow.cpp: pure planner directly uses side-effect or shared-state token ConnectByIPAddress(",
            audit.audit_state_effect_ownership(sources),
        )
        self.assertIn(
            "gbe_dota_lobby_flow.cpp: Dota reconnect network call bypasses gbe_dota_reconnect_network_adapter.cpp",
            audit.audit_state_effect_ownership(sources),
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
    STEAM_CLIENT_HEADER = """
        GBE_SharedDotaLobbyState dota_lobby_state{};
        gbe::dota_lobby_state::Store dota_lobby_store;
        gbe::dota::RuntimeState dota_runtime_state{};
        std::unique_ptr<gbe::dota::LocatorBindingGuard> dota_locator_binding;
    """
    COORDINATOR = """
        gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore() {
            return *GBE_shared_dota_lobby_store;
        }
        const GBE_DotaLootListData &GBE_GetDotaVpkLootData() { return loot; }
    """
    STEAM_CLIENT = """
    Steam_Client::Steam_Client()
    {
        steam_networking_sockets = new Steam_Networking_Sockets();
        dota_reconnect_adapter_client = new GBE_DotaReconnectNetworkAdapter();
        steam_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();
        dota_lifecycle_executor_client = new gbe::dota_lifecycle::CoordinatorExecutor();
        dota_locator_binding = std::make_unique<gbe::dota::LocatorBindingGuard>(dota_lobby_store, dota_runtime_state);
        steam_game_coordinator = new Steam_Game_Coordinator();
        steam_gameserver_networking_sockets = new Steam_Networking_Sockets();
        dota_reconnect_adapter_server = new GBE_DotaReconnectNetworkAdapter();
        steam_gameserver_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();
        dota_lifecycle_executor_server = new gbe::dota_lifecycle::CoordinatorExecutor();
        steam_gameserver_game_coordinator = new Steam_Game_Coordinator();
    }
    Steam_Client::~Steam_Client()
    {
        DEL_INST(steam_gameserver_game_coordinator);
        DEL_INST(dota_lifecycle_executor_server);
        DEL_INST(steam_gameserver_networking_sockets_serialized);
        DEL_INST(dota_reconnect_adapter_server);
        DEL_INST(steam_gameserver_networking_sockets);
        DEL_INST(steam_game_coordinator);
        DEL_INST(dota_lifecycle_executor_client);
        DEL_INST(steam_networking_sockets_serialized);
        DEL_INST(dota_reconnect_adapter_client);
        DEL_INST(steam_networking_sockets);
        dota_locator_binding.reset();
    }
    """

    def test_accepts_dependency_order_for_both_roles(self):
        self.assertEqual([], audit.audit_composition_root_lifecycle(self.STEAM_CLIENT, self.STEAM_CLIENT_HEADER, self.COORDINATOR))

    def test_rejects_coordinator_before_services(self):
        source = self.STEAM_CLIENT.replace(
            "        steam_networking_sockets = new Steam_Networking_Sockets();\n"
            "        dota_reconnect_adapter_client = new GBE_DotaReconnectNetworkAdapter();\n"
            "        steam_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();\n"
            "        dota_lifecycle_executor_client = new gbe::dota_lifecycle::CoordinatorExecutor();\n"
            "        dota_locator_binding = std::make_unique<gbe::dota::LocatorBindingGuard>(dota_lobby_store, dota_runtime_state);\n"
            "        steam_game_coordinator = new Steam_Game_Coordinator();",
            "        steam_game_coordinator = new Steam_Game_Coordinator();\n"
            "        steam_networking_sockets = new Steam_Networking_Sockets();\n"
            "        dota_reconnect_adapter_client = new GBE_DotaReconnectNetworkAdapter();\n"
            "        steam_networking_sockets_serialized = new Steam_Networking_Sockets_Serialized();\n"
            "        dota_lifecycle_executor_client = new gbe::dota_lifecycle::CoordinatorExecutor();\n"
            "        dota_locator_binding = std::make_unique<gbe::dota::LocatorBindingGuard>(dota_lobby_store, dota_runtime_state);",
        )
        self.assertIn(
            "steam_client.cpp: client GC construction must order direct sockets, reconnect adapter, serialized services, lifecycle executor, then coordinator",
            audit.audit_composition_root_lifecycle(source, self.STEAM_CLIENT_HEADER, self.COORDINATOR),
        )

    def test_rejects_service_destroyed_before_coordinator(self):
        source = self.STEAM_CLIENT.replace(
            "        DEL_INST(steam_gameserver_game_coordinator);\n"
            "        DEL_INST(dota_lifecycle_executor_server);\n"
            "        DEL_INST(steam_gameserver_networking_sockets_serialized);\n"
            "        DEL_INST(dota_reconnect_adapter_server);",
            "        DEL_INST(steam_gameserver_networking_sockets_serialized);\n"
            "        DEL_INST(dota_reconnect_adapter_server);\n"
            "        DEL_INST(dota_lifecycle_executor_server);\n"
            "        DEL_INST(steam_gameserver_game_coordinator);",
        )
        self.assertIn(
            "steam_client.cpp: gameserver GC destruction must order coordinator, lifecycle executor, serialized services, reconnect adapter, then direct sockets",
            audit.audit_composition_root_lifecycle(source, self.STEAM_CLIENT_HEADER, self.COORDINATOR),
        )

    def test_rejects_hidden_shared_lobby_store_singleton(self):
        coordinator = """
            gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore() {
                static GBE_SharedDotaLobbyState state;
                static gbe::dota_lobby_state::Store store(state, global_mutex);
                return store;
            }
            const GBE_DotaLootListData &GBE_GetDotaVpkLootData() { return loot; }
        """
        self.assertIn(
            "steam_game_coordinator.cpp: shared lobby accessor owns hidden singleton backing state",
            audit.audit_composition_root_lifecycle(self.STEAM_CLIENT, self.STEAM_CLIENT_HEADER, coordinator),
        )

    def test_rejects_retired_file_level_runtime_state(self):
        coordinator = self.COORDINATOR + "\nstatic GBE_DotaServerHelloContext GBE_last_dota_server_hello_context;"
        self.assertIn(
            "steam_game_coordinator.cpp: retired file-level Dota runtime state GBE_last_dota_server_hello_context returned",
            audit.audit_composition_root_lifecycle(self.STEAM_CLIENT, self.STEAM_CLIENT_HEADER, coordinator),
        )

    def test_rejects_retired_split_tu_runtime_state(self):
        welcome = "static bool vpk_items_loaded = false;"
        inventory = "static uint64_t equip_cache_version = 0;"
        issues = audit.audit_composition_root_lifecycle(
            self.STEAM_CLIENT,
            self.STEAM_CLIENT_HEADER,
            self.COORDINATOR,
            welcome,
            inventory,
        )
        self.assertIn(
            "gbe_dota_welcome_coordinator.cpp: retired file-level Dota runtime state vpk_items_loaded returned",
            issues,
        )
        self.assertIn(
            "gbe_dota_inventory_handlers.cpp: retired file-level Dota runtime state equip_cache_version returned",
            issues,
        )


class DependencyObjectLifecycleAuditTest(unittest.TestCase):
    def valid_sources(self):
        header = """
class ReconnectService {
    ReconnectService(
        GBE_DotaReconnectContextProvider &context_provider,
        GBE_DotaReconnectDirectConnector &direct_connector,
        GBE_DotaReconnectCallbackQueue &callback_queue);
};
class RoleContext {
    RoleContext(
        dota_lobby_state::Store &lobby_store,
        RoleDependencies dependencies);
};
// Offline-only ownership model for focused tests.
class CompositionRoot {
    GBE_SharedDotaLobbyState lobby_state_;
    std::recursive_mutex lobby_mutex_;
    dota_lobby_state::Store lobby_store_;
    std::unique_ptr<LifecycleExecutor> lifecycle_executor_;
    std::vector<dota_handler_registry::Entry> handler_registry_entries_;
    RoleContext client_;
    RoleContext server_;
};
"""
        source = 'require_dependency(lifecycle_executor_, "lifecycle_executor");'
        tests = "\n".join(
            f"void {name}();\n{name}();"
            for name in (
                "test_roots_isolate_owned_state",
                "test_client_assembly_uses_client_dependencies_only",
                "test_gameserver_assembly_uses_gameserver_dependencies_only",
                "test_delayed_work_expires_with_root_dependencies",
                "test_recreated_root_starts_without_previous_state",
            )
        )
        coordinator = """
struct Dependencies {
    class Settings *settings{};
    class Networking *network{};
    class Local_Storage *local_storage{};
    class SteamCallBacks *callbacks{};
    class RunEveryRunCB *run_every_runcb{};
    gbe::dota_lobby_state::Store *shared_lobby_store{};
    gbe::dota_handler_registry::View handler_registry{};
    gbe::dota_lifecycle::Executor *lifecycle_executor{};
};
Steam_Game_Coordinator(Dependencies dependencies, bool is_server);
"""
        client = """
GBE_SharedDotaLobbyState dota_lobby_state{};
dota_lobby_state::Store dota_lobby_store;
gbe::dota::RuntimeState dota_runtime_state{};
GBE_DotaReconnectNetworkAdapter *dota_reconnect_adapter_client{};
GBE_DotaReconnectNetworkAdapter *dota_reconnect_adapter_server{};
gbe::dota_lifecycle::CoordinatorExecutor *dota_lifecycle_executor_client{};
gbe::dota_lifecycle::CoordinatorExecutor *dota_lifecycle_executor_server{};
"""
        return header, source, tests, coordinator, client

    def audit(self, sources=None):
        return audit.audit_dependency_object_lifecycle(*(sources or self.valid_sources()))

    def test_accepts_owned_explicit_isolated_dependencies(self):
        self.assertEqual([], self.audit())

    def test_rejects_root_state_owner_removed(self):
        sources = list(self.valid_sources())
        sources[0] = sources[0].replace("    GBE_SharedDotaLobbyState lobby_state_;\n", "")
        self.assertIn(
            "gbe_dota_composition_root.h: CompositionRoot lost owned dependency 'GBE_SharedDotaLobbyState lobby_state_'",
            self.audit(sources),
        )

    def test_rejects_implicit_service_dependency(self):
        sources = list(self.valid_sources())
        sources[3] = "struct Dependencies {}; Steam_Game_Coordinator(bool is_server);"
        self.assertIn(
            "steam_game_coordinator.h: GC service lost explicit constructor dependencies",
            self.audit(sources),
        )

    def test_rejects_missing_executed_isolation_regression(self):
        sources = list(self.valid_sources())
        sources[2] = sources[2].replace("test_roots_isolate_owned_state();", "")
        self.assertIn(
            "gbe_dota_composition_root_test.cpp: missing executed lifecycle regression test_roots_isolate_owned_state",
            self.audit(sources),
        )


class MutableGcGlobalStateAuditTest(unittest.TestCase):
    def test_accepts_immutable_data_functions_and_compatibility_locators(self):
        sources = {
            "gbe_dota_locator.cpp": """
static gbe::dota_lobby_state::Store *shared_dota_lobby_store{};
static gbe::dota::RuntimeState *dota_runtime_state{};
static const registry::Entry kTable[] = {};
static constexpr uint32 GBE_kMessage = 1u;
static bool helper() { return true; }
""",
        }
        self.assertEqual([], audit.audit_mutable_gc_global_state(sources))

    def test_rejects_file_static_business_state(self):
        sources = {"gbe_dota_welcome_coordinator.cpp": "static bool vpk_items_loaded = false;"}
        self.assertIn(
            "gbe_dota_welcome_coordinator.cpp:1: mutable GC static state vpk_items_loaded is not allowlisted",
            audit.audit_mutable_gc_global_state(sources),
        )

    def test_rejects_namespace_business_state(self):
        sources = {"gbe_dota_lobby_state.cpp": "GBE_SharedDotaLobbyState shared_lobby_state{};"}
        self.assertIn(
            "gbe_dota_lobby_state.cpp:1: mutable GC global state shared_lobby_state is not allowlisted",
            audit.audit_mutable_gc_global_state(sources),
        )

    def test_rejects_unapproved_locator(self):
        sources = {"steam_game_coordinator.cpp": "static SomeService *GBE_hidden_service{};"}
        self.assertIn(
            "steam_game_coordinator.cpp:1: mutable GC static state GBE_hidden_service is not allowlisted",
            audit.audit_mutable_gc_global_state(sources),
        )


class PostLoginDispatchAuditTest(unittest.TestCase):
    def test_missing_registry_returns_complete_summary(self):
        self.assertEqual(
            (["GBE_ProductionDotaHandlerRegistry definition not found"], 0, 0),
            audit.audit_post_login_dispatch(""),
        )

    def test_accepts_registry_inventory_alignment(self):
        self.assertEqual([], audit.audit_registry_inventory_guard())

    def test_resolves_protocol_constants_for_registry_inventory(self):
        constants = audit.parse_dota_protocol_constants()
        self.assertEqual("7009", constants["GBE_kDotaJoinChatChannel"])

    def test_rejects_registry_entry_missing_from_inventory(self):
        registry_text = audit.read(audit.POST_LOGIN_REGISTRY_CPP)
        registry_text = registry_text.replace("{ 7091u, registry::RequestMode::DirectAndWrapped", "{ 9999u, registry::RequestMode::DirectAndWrapped")
        issues = audit.audit_registry_inventory_guard(registry_text=registry_text)
        self.assertTrue(any("MESSAGE_ROUTING registry emsgs" in issue for issue in issues))

    def test_rejects_inventory_row_missing_from_registry_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("| 7091 | WatchGame | WatchGame | D+W | LobbyRead | — |", "")
        issues = audit.audit_registry_inventory_guard(inventory_text=inventory_text)
        self.assertTrue(any("MESSAGE_ROUTING registry emsgs" in issue for issue in issues))

    def test_rejects_registry_handler_metadata_drift(self):
        registry_text = audit.read(audit.POST_LOGIN_REGISTRY_CPP)
        registry_text = registry_text.replace("registry::HandlerId::WatchGame", "registry::HandlerId::FindTopSourceTVGames")
        issues = audit.audit_registry_inventory_guard(registry_text=registry_text)
        self.assertTrue(any("MESSAGE_ROUTING registry metadata for 7091" in issue for issue in issues))

    def test_rejects_inventory_mode_metadata_drift(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("| 7091 | WatchGame | WatchGame | D+W | LobbyRead | — |", "| 7091 | WatchGame | WatchGame | Direct | LobbyRead | — |")
        issues = audit.audit_registry_inventory_guard(inventory_text=inventory_text)
        self.assertTrue(any("MESSAGE_ROUTING registry metadata for 7091" in issue for issue in issues))

    def test_rejects_inventory_lifecycle_metadata_drift(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("| 7091 | WatchGame | WatchGame | D+W | LobbyRead | — |", "| 7091 | WatchGame | WatchGame | D+W | LobbyLifecycle | — |")
        issues = audit.audit_registry_inventory_guard(inventory_text=inventory_text)
        self.assertTrue(any("MESSAGE_ROUTING registry metadata for 7091" in issue for issue in issues))

    def test_rejects_duplicate_registry_inventory_row(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        row = "| 7091 | WatchGame | WatchGame | D+W | LobbyRead | — |"
        inventory_text = inventory_text.replace(row, f"{row}\n{row}")
        self.assertIn(
            "MESSAGE_ROUTING registry inventory duplicates 7091",
            audit.audit_registry_inventory_guard(inventory_text=inventory_text),
        )

    def test_rejects_missing_registry_inventory_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("## 1. Registry", "## 1. moved_registry")
        self.assertIn(
            "MESSAGE_ROUTING registry inventory missing registry section",
            audit.audit_registry_inventory_guard(inventory_text=inventory_text),
        )

    def test_ignores_rows_after_registry_section_when_separator_is_missing(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "MESSAGE_ROUTING_INVENTORY.md"))
        inventory_text = inventory_text.replace("\n---\n\n## 2. 仍留在 if/fallback 的路径", "\n\n## 2. 仍留在 if/fallback 的路径", 1)
        self.assertEqual([], audit.audit_registry_inventory_guard(inventory_text=inventory_text))


class LocalSharedMergeInventoryAuditTest(unittest.TestCase):
    def test_accepts_current_local_shared_merge_inventory(self):
        self.assertEqual([], audit.audit_local_shared_merge_inventory())

    def test_rejects_missing_documented_entrypoint(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "LOCAL_LOBBY_USAGE.md"))
        inventory_text = inventory_text.replace(
            "| `restore_lobby_generation` | `gbe_dota_lobby_state.cpp` | generation restore |",
            "",
        )
        issues = audit.audit_local_shared_merge_inventory(inventory_text=inventory_text)
        self.assertIn(
            "LOCAL_LOBBY merge inventory missing restore_lobby_generation in gbe_dota_lobby_state.cpp for generation restore",
            issues,
        )

    def test_rejects_field_group_metadata_drift(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "LOCAL_LOBBY_USAGE.md"))
        inventory_text = inventory_text.replace(
            "| `restore_lobby_generation` | `gbe_dota_lobby_state.cpp` | generation restore |",
            "| `restore_lobby_generation` | `gbe_dota_lobby_state.cpp` | runtime restore |",
        )
        issues = audit.audit_local_shared_merge_inventory(inventory_text=inventory_text)
        self.assertIn(
            "LOCAL_LOBBY merge inventory missing restore_lobby_generation in gbe_dota_lobby_state.cpp for generation restore",
            issues,
        )
        self.assertIn(
            "LOCAL_LOBBY merge inventory has unexpected restore_lobby_generation in gbe_dota_lobby_state.cpp for runtime restore",
            issues,
        )

    def test_rejects_missing_merge_inventory_section(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "LOCAL_LOBBY_USAGE.md"))
        inventory_text = inventory_text.replace("## 5. Local/shared merge inventory", "## 5. moved_merge_inventory")
        self.assertIn(
            "LOCAL_LOBBY merge inventory section not found",
            audit.audit_local_shared_merge_inventory(inventory_text=inventory_text),
        )

    def test_ignores_rows_after_merge_inventory_section_when_next_heading_number_changes(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "LOCAL_LOBBY_USAGE.md"))
        inventory_text = inventory_text.replace("## 6. C1 结论", "## 7. C1 结论")
        inventory_text += "\n| `restore_lobby_generation` | `gbe_dota_lobby_state.cpp` | runtime restore |\n"
        self.assertEqual([], audit.audit_local_shared_merge_inventory(inventory_text=inventory_text))

    def test_rejects_duplicate_documented_entrypoint(self):
        inventory_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "docs", "gc", "LOCAL_LOBBY_USAGE.md"))
        row = "| `restore_lobby_generation` | `gbe_dota_lobby_state.cpp` | generation restore |"
        inventory_text = inventory_text.replace(row, f"{row}\n{row}")
        issues = audit.audit_local_shared_merge_inventory(inventory_text=inventory_text)
        self.assertIn(
            "LOCAL_LOBBY merge inventory duplicates restore_lobby_generation in gbe_dota_lobby_state.cpp for generation restore",
            issues,
        )

    def test_rejects_missing_production_entrypoint(self):
        source_text = audit.read(audit.os.path.join(audit.ROOT_DIR, "dll", "gbe_dota_lobby_state.cpp"))
        source_text = source_text.replace("restore_lobby_generation(", "restore_lobby_generation_removed(")
        issues = audit.audit_local_shared_merge_inventory(
            source_texts={"gbe_dota_lobby_state.cpp": source_text},
        )
        self.assertIn(
            "LOCAL_LOBBY merge entrypoint restore_lobby_generation definition missing from gbe_dota_lobby_state.cpp",
            issues,
        )

    def test_rejects_entrypoint_call_without_definition(self):
        source_text = """
            bool caller() {
                return restore_lobby_generation();
            }
        """
        issues = audit.audit_local_shared_merge_inventory(
            source_texts={"gbe_dota_lobby_state.cpp": source_text},
        )
        self.assertIn(
            "LOCAL_LOBBY merge entrypoint restore_lobby_generation definition missing from gbe_dota_lobby_state.cpp",
            issues,
        )

    def test_rejects_entrypoint_definition_inside_block_comment(self):
        source_text = """
            /*
            bool restore_lobby_generation() {
                return true;
            }
            */
        """
        issues = audit.audit_local_shared_merge_inventory(
            source_texts={"gbe_dota_lobby_state.cpp": source_text},
        )
        self.assertIn(
            "LOCAL_LOBBY merge entrypoint restore_lobby_generation definition missing from gbe_dota_lobby_state.cpp",
            issues,
        )

    def test_rejects_entrypoint_definition_inside_line_comment(self):
        source_text = """
            // bool restore_lobby_generation() { return true; }
        """
        issues = audit.audit_local_shared_merge_inventory(
            source_texts={"gbe_dota_lobby_state.cpp": source_text},
        )
        self.assertIn(
            "LOCAL_LOBBY merge entrypoint restore_lobby_generation definition missing from gbe_dota_lobby_state.cpp",
            issues,
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
            "gbe_dota_post_login_dispatcher.cpp": """
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
            "gbe_dota_post_login_dispatcher.cpp": """
                bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest(const Context &context) {
                    static const registry::Entry kTable[] = {};
                    return registry::find_entry(kTable, 0, context.inner_emsg, context.path);
                }
            """,
        }
        issues = audit.audit_architecture_boundaries(sources)
        self.assertIn(
            "gbe_dota_post_login_dispatcher.cpp: post-login dispatcher owns a registry table instead of consuming the injected view",
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
            "gbe_dota_post_login_dispatcher.cpp": """
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
            "gbe_dota_post_login_handlers.cpp: parallel typed post-login registry returned outside gbe_dota_post_login_dispatcher.cpp",
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
            - run: bash tools/run_gc_verification.sh --full --base-sha "${{ github.event.pull_request.base.sha }}"
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

    def test_accepts_blocking_full_production_and_tsan_layers(self):
        self.assertEqual(
            [],
            audit.audit_layered_ci_gates(
                self.WORKFLOW,
                self.VERIFICATION,
                self.OFFLINE,
                self.TSAN,
            ),
        )

    def test_rejects_fast_job_and_missing_production_or_tsan_boundaries(self):
        workflow = self.WORKFLOW.replace("--full", "--fast")
        workflow = workflow.replace("matrix_cfg: '[\"release\"]'", "matrix_cfg: '[\"debug\"]'", 1)
        workflow = workflow.replace("CXX: clang++", "CXX: c++")
        issues = audit.audit_layered_ci_gates(
            workflow,
            self.VERIFICATION.replace("python3 tools/_audit_gc_refactor.py", ""),
            self.OFFLINE,
            self.TSAN,
        )
        self.assertIn(
            "emu-pull-request.yml: full GC layer must run run_gc_verification.sh --full with the PR base SHA",
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
            "run_gc_verification.sh: full layer is missing architecture audit execution",
            issues,
        )


class CiFailureLocalizationAuditTest(unittest.TestCase):
    WORKFLOW = """
  emu-win-release:
    name: "win"
    uses: "./.github/workflows/emu-build-all-win.yml"
    with:
      continue_on_error: false
  emu-linux-release:
    name: "linux"
    uses: "./.github/workflows/emu-build-all-linux.yml"
    with:
      continue_on_error: false
  gc-verification:
    name: "gc verification"
    steps:
      - name: "Run full GC verification"
        run: bash tools/run_gc_verification.sh --full --base-sha base
  gc-tsan:
    name: "gc thread sanitizer"
    steps:
      - name: "Run GC ThreadSanitizer tests"
        run: bash tools/run_gc_tsan_tests.sh
"""
    VERIFICATION = "set -euo pipefail\nGC verification passed"
    TSAN = "set -euo pipefail"

    def test_accepts_named_fail_fast_ci_boundaries(self):
        self.assertEqual(
            [],
            audit.audit_ci_failure_localization(self.WORKFLOW, self.VERIFICATION, self.TSAN),
        )

    def test_rejects_unnamed_tsan_execution_step(self):
        workflow = self.WORKFLOW.replace('      - name: "Run GC ThreadSanitizer tests"\n', "")
        self.assertIn(
            'emu-pull-request.yml: gc-tsan is missing failure localization token name: "Run GC ThreadSanitizer tests"',
            audit.audit_ci_failure_localization(workflow, self.VERIFICATION, self.TSAN),
        )

    def test_rejects_non_fail_fast_gate_script(self):
        issues = audit.audit_ci_failure_localization(self.WORKFLOW, "GC verification passed", self.TSAN)
        self.assertIn(
            "run_gc_verification.sh: CI gate must fail fast with set -euo pipefail",
            issues,
        )




if __name__ == "__main__":
    unittest.main()
