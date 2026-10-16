"""Exact landed-B2 parent, B3 scope and immutable historical/whole-case boundaries."""
import json
import subprocess
import unittest
from unittest.mock import patch
import check_m0_rc1_pins as pins

BASE = "c76958e8398e5eefde9b77d095a320cda4aac51a"
SNAPSHOT = "7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79"

class M1B3Custody(unittest.TestCase):
    def test_complete_candidate(self):
        self.assertTrue(pins.check_m1_b3(BASE, SNAPSHOT))

    def test_moved_main_rejected(self):
        with self.assertRaises(AssertionError): pins.check_m1_b3("f" * 40, SNAPSHOT)

    def test_missing_b2_ancestor_rejected(self):
        with self.assertRaises((AssertionError, subprocess.CalledProcessError)):
            pins.check_m1_b3(BASE, pins.M1_B2_BASE)

    def test_extra_gpu_path_and_missing_component_rejected(self):
        real = pins.git
        for victim in ("src/graphics/gpu.cpp", "src/motion/provenance.cpp"):
            def changed(*args):
                value = real(*args)
                if args[:3] == ("diff", "--name-only", BASE):
                    return value + "\n" + victim if victim.startswith("src/graphics") else "\n".join(p for p in value.splitlines() if p != victim)
                return value
            with self.subTest(path=victim), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_b3(BASE, SNAPSHOT)

    def test_frozen_abi_oracle_and_old_native_tests_rejected(self):
        real = pins.git
        for path in ("include/ars/ars.h", "src/abi/unsupported.cpp", "cmake/ars_verify_exports.cmake", "tests/security/source_authority_tests.cpp"):
            def changed(*args):
                return "0" * 40 if args == ("rev-parse", SNAPSHOT + ":" + path) else real(*args)
            with self.subTest(path=path), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_b3(BASE, SNAPSHOT)

    def test_historical_b2_custody_dilution_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            return value.replace("pins.check_m1_b2(BASE, SNAPSHOT)", "True") if args == ("show", SNAPSHOT + ":tests/contracts/test_m1_b2_custody.py") else value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_b3(BASE, SNAPSHOT)

    def test_historical_gate_change_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            return value.replace('require(base == M1_B2_BASE,', 'require(True,') if args == ("show", SNAPSHOT + ":tests/contracts/check_m0_rc1_pins.py") else value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_b3(BASE, SNAPSHOT)

    def test_whole_case_pass_or_changed_oracle_rejected(self):
        real = pins.git
        for field in ("whole_case_verdict", "oracle", "whole_case_pass_count"):
            def changed(*args):
                value = real(*args)
                if args == ("show", SNAPSHOT + ":docs/audits/M1_B_CASE_EXECUTION.json"):
                    obj = json.loads(value)
                    if field == "oracle": obj["cases"][0]["original"][field] = "weakened"
                    elif field == "whole_case_pass_count": obj[field] = 1
                    else: obj["cases"][0][field] = "PASS"
                    return json.dumps(obj)
                return value
            with self.subTest(field=field), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_b3(BASE, SNAPSHOT)

    def test_public_mock_export_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            return value + "\nARS_API void public_mock_reset();\n" if args == ("show", SNAPSHOT + ":src/motion/publish_gate.hpp") else value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_b3(BASE, SNAPSHOT)

if __name__ == "__main__": unittest.main()
