"""C2 stack custody and independently falsified boundary controls."""
import json
import unittest
from unittest.mock import patch
import check_m0_rc1_pins as pins

BASE = "9f5d6b8a634138e8dedae8385dcb783cd385b6c8"
SNAPSHOT = "0eb1560e0cdfa5a7f44b77efc8dbf559dbc014ef"

class M1C2Custody(unittest.TestCase):
    def test_complete_candidate(self):
        self.assertTrue(pins.check_m1_c2(BASE, SNAPSHOT))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError): pins.check_m1_c2("f"*40, SNAPSHOT)

    def test_extra_path_and_missing_parser_rejected(self):
        real = pins.git
        for missing in (False, True):
            def changed(*args):
                value = real(*args)
                if args[:3] == ("diff", "--name-only", BASE):
                    return "\n".join(p for p in value.splitlines() if p != "src/assets/glb_reader.cpp") if missing else value+"\nsrc/graphics/gpu.cpp"
                return value
            with self.subTest(missing=missing), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c2(BASE, SNAPSHOT)

    def test_frozen_boundaries_rejected(self):
        real = pins.git
        for path in ("include/ars/ars.h", "docs/audits/M1_B_CASE_EXECUTION.json"):
            def changed(*args):
                return "0"*40 if args == ("rev-parse", SNAPSHOT+":"+path) else real(*args)
            with self.subTest(path=path), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c2(BASE, SNAPSHOT)

    def test_historical_gate_dilution_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            return value.replace('require(base == M1_C1_BASE,', 'require(True,') if args == ("show", SNAPSHOT+":tests/contracts/check_m0_rc1_pins.py") else value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_c2(BASE, SNAPSHOT)

    def test_snapshot_test_dilution_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            return value.replace('pins.check_m1_c1(BASE, SNAPSHOT)', 'True') if args == ("show", SNAPSHOT+":tests/contracts/test_m1_c_custody.py") else value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_c2(BASE, SNAPSHOT)

    def test_unapproved_stack_and_dependency_substitution_rejected(self):
        real = pins.git
        for field in ("approval", "parent", "dependency", "activation"):
            def changed(*args):
                value = real(*args)
                if args == ("show", SNAPSHOT+":docs/M1_C_DEPENDENCY_LOCK.json"):
                    obj = json.loads(value)
                    if field == "approval": obj["c2_stack"]["owner_unmerged_stack_approved"] = False
                    if field == "parent": obj["c2_stack"]["parent_sha"] = "0"*40
                    if field == "dependency": obj["dependencies"][0]["commit"] = "0"*40
                    if field == "activation": obj["public_activation"] = True
                    return json.dumps(obj)
                return value
            with self.subTest(field=field), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c2(BASE, SNAPSHOT)

    def test_fabricated_g02_or_B_pass_rejected(self):
        real = pins.git
        for field, value in (("full_g02_verdict", "PASS"), ("B_whole_case_PASS", 87), ("public_activation", True)):
            def changed(*args):
                result = real(*args)
                if args == ("show", SNAPSHOT+":docs/audits/M1_C_CASE_EXECUTION.json"):
                    obj = json.loads(result); obj[field] = value; return json.dumps(obj)
                return result
            with self.subTest(field=field), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c2(BASE, SNAPSHOT)

if __name__ == "__main__": unittest.main()
