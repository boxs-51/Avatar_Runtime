"""C1 exact source boundary, helper amendment and preserved B3 history."""
import json
import unittest
from unittest.mock import patch
import check_m0_rc1_pins as pins

BASE = "7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79"
SNAPSHOT = "9f5d6b8a634138e8dedae8385dcb783cd385b6c8"

class M1C1Custody(unittest.TestCase):
    def test_complete_candidate(self):
        self.assertTrue(pins.check_m1_c1(BASE, SNAPSHOT))

    def test_moved_target_rejected(self):
        with self.assertRaises(AssertionError): pins.check_m1_c1("f"*40, SNAPSHOT)

    def test_extra_gpu_and_missing_worker_rejected(self):
        real = pins.git
        for path in ("src/graphics/gpu.cpp", "src/validation/schema_worker_linux.cpp"):
            def changed(*args):
                value = real(*args)
                if args[:3] == ("diff", "--name-only", BASE):
                    return value+"\n"+path if "graphics" in path else "\n".join(p for p in value.splitlines() if p != path)
                return value
            with self.subTest(path=path), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c1(BASE, SNAPSHOT)

    def test_public_abi_and_B_inventory_changes_rejected(self):
        real = pins.git
        for path in ("src/abi/unsupported.cpp", "docs/audits/M1_B_CASE_EXECUTION.json"):
            def changed(*args):
                return "0"*40 if args == ("rev-parse", SNAPSHOT + ":"+path) else real(*args)
            with self.subTest(path=path), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c1(BASE, SNAPSHOT)

    def test_historical_B3_dilution_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            if args == ("show", SNAPSHOT + ":tests/contracts/test_m1_b3_custody.py"):
                return value.replace("pins.check_m1_b3(BASE, SNAPSHOT)", "True")
            return value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_c1(BASE, SNAPSHOT)

    def test_historical_gate_dilution_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            return value.replace('require(base == M1_B3_BASE,', 'require(True,') if args == ("show", SNAPSHOT + ":tests/contracts/check_m0_rc1_pins.py") else value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_c1(BASE, SNAPSHOT)

    def test_unapproved_helper_and_activation_rejected(self):
        real = pins.git
        for field in ("public_activation", "approval"):
            def changed(*args):
                value = real(*args)
                if args == ("show", SNAPSHOT + ":docs/M1_C_DEPENDENCY_LOCK.json"):
                    obj = json.loads(value)
                    if field == "approval": obj["material_decision_required"]["status"] = "PROPOSAL_NOT_AUTHORIZED"
                    else: obj[field] = True
                    return json.dumps(obj)
                return value
            with self.subTest(field=field), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c1(BASE, SNAPSHOT)

    def test_dependency_substitution_rejected(self):
        real = pins.git
        def changed(*args):
            value = real(*args)
            if args == ("show", SNAPSHOT + ":docs/M1_C_DEPENDENCY_LOCK.json"):
                obj = json.loads(value);obj["dependencies"][0]["archive_sha256"] = "f"*64;return json.dumps(obj)
            return value
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_c1(BASE, SNAPSHOT)

    def test_public_export_or_full_asset_pass_rejected(self):
        real = pins.git
        for path in ("src/assets/validated_cpu_model.hpp", "docs/audits/M1_C_CASE_EXECUTION.json"):
            def changed(*args):
                value = real(*args)
                if args == ("show", SNAPSHOT + ":"+path):
                    if path.endswith(".hpp"): return value+"\nARS_API void mock_load();\n"
                    obj = json.loads(value);obj["full_g02_verdict"] = "PASS";return json.dumps(obj)
                return value
            with self.subTest(path=path), patch.object(pins, "git", side_effect=changed):
                with self.assertRaises(AssertionError): pins.check_m1_c1(BASE, SNAPSHOT)

if __name__ == "__main__": unittest.main()
