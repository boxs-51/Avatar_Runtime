"""M1-A source scope is separate from immutable historical M0 composition."""
import subprocess
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins

BASE = "a307c6775388a507aeacb5fbeb09479633c20f58"
SNAPSHOT = "ab21908310d48466b27bb06c92ea1623353c79f5"


class M1ACustody(unittest.TestCase):
    def test_complete_candidate(self):
        self.assertTrue(pins.check_m1_a(BASE, SNAPSHOT))

    def test_moved_main_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m1_a("f" * 40, SNAPSHOT)

    def test_missing_m0_ancestry_rejected(self):
        with self.assertRaises((AssertionError, subprocess.CalledProcessError)):
            pins.check_m1_a(BASE, "637bdae7373aecbf50b18834f3118421d921fe3a")

    def test_extra_runtime_source_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            if args[:3] == ("diff", "--name-only", BASE):
                return value + "\nsrc/core/registry.cpp"
            return value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_a(BASE, SNAPSHOT)

    def test_missing_export_checker_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            if args[:3] == ("diff", "--name-only", BASE):
                return "\n".join(p for p in value.splitlines() if p != "cmake/ars_verify_exports.cmake")
            return value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_a(BASE, SNAPSHOT)

    def test_header_edit_rejected(self):
        real = pins.git
        def changed(*args):
            if args == ("rev-parse", SNAPSHOT + ":include/ars/ars.h"):
                return "0" * 40
            return real(*args)
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_a(BASE, SNAPSHOT)

    def test_unlisted_historical_test_change_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            if args[:3] == ("diff", "--name-only", BASE):
                return value + "\ntests/contracts/test_g00_final_guards.py"
            return value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_a(BASE, SNAPSHOT)


if __name__ == "__main__":
    unittest.main()
