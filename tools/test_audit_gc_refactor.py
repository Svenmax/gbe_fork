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


if __name__ == "__main__":
    unittest.main()
