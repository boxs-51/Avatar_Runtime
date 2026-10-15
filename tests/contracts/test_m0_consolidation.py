"""Exact-source custody checks for the consolidated M0 candidate, not merge permission."""
import subprocess
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins

MAIN = "637bdae7373aecbf50b18834f3118421d921fe3a"
FROZEN = "fe3e680a44ac4e71ef57dc177e73ae31198fcbd0"
PR15 = "7cf306dd783edd7e155ac26517c4b99b8b449552"


class ConsolidatedCustody(unittest.TestCase):
    def test_complete_candidate_preserves_exact_sources(self):
        self.assertTrue(pins.check_consolidation(MAIN, "HEAD"))

    def test_changed_target_requires_fresh_review(self):
        with self.assertRaises(AssertionError):
            pins.check_consolidation("a" * 40, "HEAD")

    def test_original_pr15_cannot_impersonate_consolidation(self):
        with self.assertRaises((AssertionError, subprocess.CalledProcessError)):
            pins.check_consolidation(MAIN, PR15)

    def test_hidden_runtime_source_rejected(self):
        real_git = pins.git
        def altered(*args):
            result = real_git(*args)
            if args[:3] == ("diff", "--name-only", FROZEN):
                return result + "\nsrc/core/unauthorized.cpp"
            return result
        with patch.object(pins, "git", side_effect=altered):
            with self.assertRaises(AssertionError):
                pins.check_consolidation(MAIN, "HEAD")

    def test_missing_required_evidence_path_rejected(self):
        real_git = pins.git
        def altered(*args):
            result = real_git(*args)
            if args[:3] == ("diff", "--name-only", FROZEN):
                return "\n".join(p for p in result.splitlines()
                                 if p != "docs/M1_A_SCOPE_V0.1.md")
            return result
        with patch.object(pins, "git", side_effect=altered):
            with self.assertRaises(AssertionError):
                pins.check_consolidation(MAIN, "HEAD")

    def test_governance_cannot_be_silently_rewritten(self):
        real_git = pins.git
        def altered(*args):
            if args == ("rev-parse", "HEAD:docs/governance/ARS_M5_MERGE_HISTORY.md"):
                return "0" * 40
            return real_git(*args)
        with patch.object(pins, "git", side_effect=altered):
            with self.assertRaises(AssertionError):
                pins.check_consolidation(MAIN, "HEAD")


if __name__ == "__main__":
    unittest.main()
