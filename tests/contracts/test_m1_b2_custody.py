"""Owner-approved B2 cannot dilute B1, public activation or whole-case evidence."""
import json
import subprocess
import unittest
from unittest.mock import patch
import check_m0_rc1_pins as pins

BASE = "535663960c33c0ebe49481a8681b8625c336fafe"
SNAPSHOT = "c76958e8398e5eefde9b77d095a320cda4aac51a"

class M1B2Custody(unittest.TestCase):
    def test_complete_candidate(self):
        self.assertTrue(pins.check_m1_b2(BASE, SNAPSHOT))

    def test_moved_main_rejected(self):
        with self.assertRaises(AssertionError): pins.check_m1_b2("f" * 40, SNAPSHOT)

    def test_missing_ancestor_rejected(self):
        with self.assertRaises((AssertionError, subprocess.CalledProcessError)):
            pins.check_m1_b2(BASE, "a307c6775388a507aeacb5fbeb09479633c20f58")

    def test_extra_b3_path_and_missing_backend_rejected(self):
        real = pins.git
        for victim in ("src/motion/provenance.cpp", "src/security/os_entropy_linux.cpp"):
            def changed(*args):
                value = real(*args)
                if args[:3] == ("diff", "--name-only", BASE):
                    return value + "\n" + victim if victim.startswith("src/motion") else "\n".join(p for p in value.splitlines() if p != victim)
                return value
            with self.subTest(path=victim), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_b2(BASE, SNAPSHOT)

    def test_header_and_public_activation_mutation_rejected(self):
        real = pins.git
        for path in ("include/ars/ars.h", "src/abi/unsupported.cpp", "src/abi/runtime_api.cpp"):
            def changed(*args):
                return "0" * 40 if args == ("rev-parse", SNAPSHOT + ":" + path) else real(*args)
            with self.subTest(path=path), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_b2(BASE, SNAPSHOT)

    def test_historical_b1_test_dilution_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            if args == ("show", SNAPSHOT + ":tests/contracts/test_m1_b_custody.py"):
                return value.replace('pins.check_m1_b1(BASE, SNAPSHOT)', 'True')
            return value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_b2(BASE, SNAPSHOT)

    def test_whole_case_pass_and_original_oracle_change_rejected(self):
        real = pins.git
        for field in ("whole_case_verdict", "oracle"):
            def changed(*args):
                value = real(*args)
                if args == ("show", SNAPSHOT + ":docs/audits/M1_B_CASE_EXECUTION.json"):
                    obj = json.loads(value)
                    if field == "whole_case_verdict": obj["cases"][0][field] = "PASS"
                    else: obj["cases"][0]["original"][field] = "weakened"
                    return json.dumps(obj)
                return value
            with self.subTest(field=field), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_b2(BASE, SNAPSHOT)

    def test_public_mock_export_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            if args == ("show", SNAPSHOT + ":src/security/security_clock.hpp"):
                return value + "\nARS_API void public_mock_reset();\n"
            return value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_b2(BASE, SNAPSHOT)

if __name__ == "__main__": unittest.main()
