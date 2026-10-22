#!/usr/bin/env python3
"""G00 security inventory completeness/owner/stage oracle check; not a runtime test."""
import copy
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOC = ROOT / 'docs/reference/P0_SECURITY_REGRESSION_GATES_V0.1.md'
DATA = ROOT / 'docs/reference/G00_SECURITY_87_CASE_INVENTORY.json'
OS = {"ubuntu-24.04/GCC", "ubuntu-24.04/Clang", "windows-2022/MSVC"}

def from_normative_text(raw):
    """Fail closed on *physical* row/heading duplicates, before dict reduction."""
    rows = re.findall(
        r"^\|\s*((?:HDL|SRC)-\d{3})\s*\|\s*([^|\n]+)\|\s*([^|\n]+)\|",
        raw, re.M)
    races = list(re.finditer(r"^### RACE-([A-Z]):\s*([^\n]*)", raw, re.M))
    hdl = [r for r in rows if r[0].startswith("HDL-")]
    src = [r for r in rows if r[0].startswith("SRC-")]
    ids = [r[0] for r in rows] + ["RACE-"+r.group(1) for r in races]
    if (len(rows) != 64 or len(hdl) != 19 or len(src) != 45
            or len(races) != 23 or len(ids) != len(set(ids))):
        raise AssertionError("Duplicate/missing normative HDL/SRC/RACE case row")
    source = {name: {"scenario": scenario.strip(), "oracle": oracle.strip()}
              for name, scenario, oracle in rows}
    for idx, hit in enumerate(races):
        end = (races[idx+1].start() if idx+1<len(races)
               else raw.index("## 5. Behavioral", hit.start()))
        segment = raw[hit.start():end]
        assertions = [line.strip()[7:].strip() for line in segment.splitlines()
                      if line.strip().startswith("ASSERT:")]
        if not assertions or any(not item for item in assertions):
            raise AssertionError("Missing normative RACE assertion: "+hit.group(1))
        source["RACE-"+hit.group(1)] = {
            "scenario": hit.group(2).strip(), "oracle": " | ".join(assertions)}
    return source

GPU_CASES = frozenset(("HDL-009", "HDL-015", "HDL-019", "RACE-D", "RACE-J"))
SOURCE_PATH = "docs/P0_SECURITY_REGRESSION_GATES_V0.1.md"
STAGE = "M1-B/G07-G08"
GPU = "Windows D3D11 GPU/M1-D"

def validate(data, source):
    expected = (["HDL-%03d"%n for n in range(1,20)] +
                ["SRC-%03d"%n for n in range(1,46)] +
                ["RACE-"+chr(n) for n in range(ord("A"),ord("W")+1)])
    if set(source) != set(expected) or len(source) != 87:
        raise AssertionError("Normative Markdown must supply exactly 87 unique cases")
    entries = data["cases"]
    ids = [e["id"] for e in entries]
    if len(entries)!=87 or len(set(ids))!=87 or set(ids)!=set(expected):
        raise AssertionError("Case inventory missing/duplicate/extra")
    for entry in entries:
        name = entry["id"]
        if (entry["oracle"] != source[name]["oracle"]
                or entry["scenario"] != source[name]["scenario"]):
            raise AssertionError("Scenario/oracle source divergence: "+name)
        owner = ("Runtime handle/registry" if name.startswith("HDL-") else
                 "Motion source/authorization/pose" if name.startswith("SRC-") else
                 "Concurrency + Graphics lifetime" if name in ("RACE-D", "RACE-J") else
                 "Concurrency/source authority/pose")
        mode = ("deterministic interleaving + >=1000 randomized iterations"
                if name.startswith("RACE-") else "production negative regression")
        platforms = set(OS)
        if name in GPU_CASES:
            platforms.add(GPU)
        if entry["owner_scope"] != owner or entry["source"] != SOURCE_PATH:
            raise AssertionError("Custody/source changed: "+name)
        if entry["test_mode"] != mode:
            raise AssertionError("Test-mode weakened: "+name)
        if set(entry["required_platforms"]) != platforms or len(entry["required_platforms"]) != len(platforms):
            raise AssertionError("OS/GPU matrix weakened: "+name)
        if entry["required_stage"] != STAGE or entry["evidence_status"] != "HOLD_RUNTIME":
            raise AssertionError("Production security gate falsely waived: "+name)
    if (data["count"] != 87
            or data["status"] != "DESIGN_INVENTORY_ONLY_NOT_RUNTIME_TEST_EVIDENCE"
            or data["source_contract"] != SOURCE_PATH):
        raise AssertionError("Improper G00 vs runtime contract")
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
