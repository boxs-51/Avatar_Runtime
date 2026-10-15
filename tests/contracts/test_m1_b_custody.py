"""B1 source remains bounded independently of frozen M0 and M1-A evidence."""
import json
import subprocess
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins

BASE = "ab21908310d48466b27bb06c92ea1623353c79f5"
SNAPSHOT = "535663960c33c0ebe49481a8681b8625c336fafe"


class M1B1Custody(unittest.TestCase):
    def test_complete_candidate(self):
        self.assertTrue(pins.check_m1_b1(BASE, SNAPSHOT))

    def test_moved_base_rejected(self):
        with self.assertRaises(AssertionError): pins.check_m1_b1("f" * 40, SNAPSHOT)

    def test_missing_ancestor_rejected(self):
        with self.assertRaises((AssertionError, subprocess.CalledProcessError)):
            pins.check_m1_b1(BASE, "a307c6775388a507aeacb5fbeb09479633c20f58")

    def test_scope_paths_and_missing_native_test_rejected(self):
        real = pins.git
        for victim in ("src/motion/source_registry.cpp", "tests/security/runtime_lifecycle_tests.cpp"):
            def changed(*args):
                value = real(*args)
                if args[:3] == ("diff", "--name-only", BASE):
                    return value + "\n" + victim if victim.startswith("src/") else "\n".join(p for p in value.splitlines() if p != victim)
                return value
            with self.subTest(path=victim),patch.object(pins,"git",side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_b1(BASE,SNAPSHOT)

    def test_frozen_header_mutation_rejected(self):
        real = pins.git
        def changed(*args):
            return "0" * 40 if args == ("rev-parse", SNAPSHOT + ":include/ars/ars.h") else real(*args)
        with patch.object(pins,"git",side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_b1(BASE,SNAPSHOT)

    def test_full_security_verdicts_cannot_be_fabricated(self):
        real = pins.git
        def changed(*args):
            value=real(*args)
            if args == ("show", SNAPSHOT + ":docs/audits/M1_B_CASE_EXECUTION.json"):
                obj=json.loads(value); obj["cases"][0]["whole_case_verdict"]="PASS"
                return json.dumps(obj)
            return value
        with patch.object(pins,"git",side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_b1(BASE,SNAPSHOT)


if __name__ == "__main__": unittest.main()
