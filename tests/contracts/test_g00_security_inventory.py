#!/usr/bin/env python3
"""G00 security inventory completeness/owner/stage oracle check; not a runtime test."""
import copy
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOC = ROOT / "docs/P0_SECURITY_REGRESSION_GATES_V0.1.md"
DATA = ROOT / "docs/G00_SECURITY_87_CASE_INVENTORY.json"
OS = {"ubuntu-24.04/GCC", "ubuntu-24.04/Clang", "windows-2022/MSVC"}

def from_normative_text(raw):
    rows = re.findall(r"^\|\s*(HDL-\d{3}|SRC-\d{3})\s*\|\s*([^|]+)\|\s*([^|]+)\|", raw, re.M)
    source = {item.strip(): oracle.strip() for item, _, oracle in rows}
    races = list(re.finditer(r"^### RACE-([A-W]):\s*([^\n]*)", raw, re.M))
    for idx, hit in enumerate(races):
        segment = raw[hit.start(): races[idx+1].start() if idx+1<len(races) else raw.index("## 5. Behavioral", hit.start())]
        assertions = [line.strip()[7:].strip() for line in segment.splitlines() if line.strip().startswith("ASSERT:")]
        if not assertions:
            raise ValueError("missing normative assertion for " + hit.group(1))
        source["RACE-" + hit.group(1)] = " | ".join(assertions)
    return source

def validate(data, source):
    expected = (["HDL-%03d"%n for n in range(1,20)] +
                ["SRC-%03d"%n for n in range(1,46)] +
                ["RACE-"+chr(n) for n in range(ord("A"),ord("W")+1)])
    if set(source) != set(expected) or len(source) != 87:
        raise AssertionError("Normative Markdown not exactly 87 unique cases")
    entries = data["cases"]
    ids = [e["id"] for e in entries]
    if len(entries) != 87 or len(set(ids)) != 87 or set(ids) != set(expected):
        raise AssertionError("Case inventory missing/duplicate/extra")
    for entry in entries:
        if entry["oracle"] != source[entry["id"]]:
            raise AssertionError("Oracle diverges: " + entry["id"])
        if not entry["scenario"] or not entry["owner_scope"]:
            raise AssertionError("Missing scenario/custody: " + entry["id"])
        if entry["required_stage"] != "M1-B/G07-G08":
            raise AssertionError("Production security gate waived: " + entry["id"])
        if entry["evidence_status"] != "HOLD_RUNTIME":
            raise AssertionError("Invented runtime PASS: " + entry["id"])
        if not OS.issubset(set(entry["required_platforms"])):
            raise AssertionError("Missing Linux/Windows matrix: " + entry["id"])
    if data["count"] != 87 or "NOT_RUNTIME_TEST_EVIDENCE" not in data["status"]:
        raise AssertionError("Improper G00 vs runtime stage claim")
    return True

class InventoryContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(DATA.read_text(encoding="utf-8"))
        cls.source = from_normative_text(DOC.read_text(encoding="utf-8"))
    def test_complete_exact_owner_oracle_matrix(self):
        self.assertTrue(validate(self.data, self.source))
    # P1 RED-first: the Markdown source must not silently duplicate a case ID.
    def test_raw_duplicate_hdl_row_fails_closed(self):
        raw = DOC.read_text(encoding="utf-8")
        source_line = next(line for line in raw.splitlines()
                           if line.startswith("| HDL-001 |"))
        with self.assertRaises((AssertionError, ValueError)):
            from_normative_text(raw.replace(source_line,
                                             source_line + "\n" + source_line))
    def test_raw_duplicate_race_heading_fails_closed(self):
        raw = DOC.read_text(encoding="utf-8")
        with self.assertRaises((AssertionError, ValueError)):
            from_normative_text(raw.replace("### RACE-K:",
                                             "### RACE-K:\n    ASSERT: duplicate\n\n### RACE-K:"))
    # P2: expected scenario/custody/test-mode/platforms are part of oracle.
    def test_mutated_scenario_must_fail(self):
        bad = copy.deepcopy(self.data)
        bad["cases"][0]["scenario"] = "silent acceptance"
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_mutated_custody_must_fail(self):
        bad = copy.deepcopy(self.data)
        bad["cases"][0]["owner_scope"] = "Nobody"
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_downgraded_race_test_mode_fails(self):
        bad = copy.deepcopy(self.data)
        hit = next(x for x in bad["cases"] if x["id"]=="RACE-K")
        hit["test_mode"] = "one mock test"
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_gpu_sensitive_case_without_gpu_runner_fails(self):
        bad = copy.deepcopy(self.data)
        hit = next(x for x in bad["cases"] if x["id"]=="HDL-009")
        hit["required_platforms"].remove("Windows D3D11 GPU/M1-D")
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_missing_case_fails_closed(self):
        bad = copy.deepcopy(self.data); bad["cases"].pop()
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_duplicate_case_fails_closed(self):
        bad=copy.deepcopy(self.data); bad["cases"][-1] = bad["cases"][0]
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_weakened_oracle_fails_closed(self):
        bad=copy.deepcopy(self.data); bad["cases"][0]["oracle"] = "accept"
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_fabricated_runtime_green_fails_closed(self):
        bad=copy.deepcopy(self.data); bad["cases"][0]["evidence_status"] = "PASS"
        with self.assertRaises(AssertionError): validate(bad,self.source)
    def test_removed_windows_coverage_fails_closed(self):
        bad=copy.deepcopy(self.data); bad["cases"][0]["required_platforms"]=["ubuntu-24.04/GCC"]
        with self.assertRaises(AssertionError): validate(bad,self.source)

if __name__ == "__main__":
    unittest.main()
