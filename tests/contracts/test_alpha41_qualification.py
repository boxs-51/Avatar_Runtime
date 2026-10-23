"""Negative oracles for #41 receipt/corpus custody, not runtime substitutes."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("a41", ROOT / "scripts/verify_asset_runtime_qualification.py")
qualification = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualification)


class QualificationGuards(unittest.TestCase):
    rows = [{"file": "valid.arschar", "expected": 0, "family": "valid"},
            {"file": "hostile.arschar", "expected": 3, "family": "invalid"}]
    good = "CASE valid.arschar 0 PASS\nCASE hostile.arschar 3 PASS\nASSET_QUALIFICATION_PASS cases=2 live=0\n"

    def test_complete_receipt_has_every_exact_case(self):
        result = qualification.inspect_cases(self.good, self.rows)
        self.assertEqual([r["actual"] for r in result], [0, 3])

    def test_missing_duplicate_or_wrong_status_never_pass(self):
        for text in (self.good.replace("CASE hostile.arschar 3 PASS\n", ""),
                     self.good + "CASE hostile.arschar 3 PASS\n",
                     self.good.replace("hostile.arschar 3", "hostile.arschar 0")):
            with self.subTest(text=text), self.assertRaises(RuntimeError):
                qualification.inspect_cases(text, self.rows)

    def test_incomplete_or_nonzero_resource_summary_rejected(self):
        for tail in ("", "ASSET_QUALIFICATION_PASS cases=1 live=0", "ASSET_QUALIFICATION_PASS cases=2 live=1",
                     "prefix ASSET_QUALIFICATION_PASS cases=2 live=0\n", "ASSET_QUALIFICATION_PASS cases=2 live=0"):
            with self.subTest(tail=tail), self.assertRaises(RuntimeError):
                qualification.inspect_cases(self.good.split("ASSET_QUALIFICATION_PASS")[0] + tail, self.rows)

    def test_frozen_corpus_bytes_not_implementation_output(self):
        scratch = ROOT / "build"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder = Path(temporary)
            oracle = folder / "tests/assets/fixtures"
            oracle.mkdir(parents=True)
            data = b"generated synthetic fixture"
            (folder / "valid.arschar").write_bytes(data)
            record = {"file": "valid.arschar", "bytes": len(data),
                      "sha256": hashlib.sha256(data).hexdigest(), "family": "synthetic", "expected": "OK"}
            manifest = oracle / "manifest.json"
            manifest.write_text(json.dumps([record]))
            with patch.object(qualification, "ROOT", folder):
                self.assertEqual(len(qualification.corpus_rows(folder)), 1)
                (folder / "valid.arschar").write_bytes(data[:-1] + b"!")
                with self.assertRaisesRegex(RuntimeError, "CORPUS_DRIFT"):
                    qualification.corpus_rows(folder)
                (folder / "valid.arschar").write_bytes(data)
                manifest.write_text(json.dumps([record, record]))
                with self.assertRaisesRegex(RuntimeError, "EMPTY_OR_DUPLICATE_CORPUS"):
                    qualification.corpus_rows(folder)

    def test_empty_or_duplicate_corpus_is_failure(self):
        scratch = ROOT / "build"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder = Path(temporary)
            oracle = folder / "tests/assets/fixtures"
            oracle.mkdir(parents=True)
            manifest = oracle / "manifest.json"
            with patch.object(qualification, "ROOT", folder):
                manifest.write_text("[]")
                with self.assertRaisesRegex(RuntimeError, "EMPTY_OR_DUPLICATE_CORPUS"):
                    qualification.corpus_rows(folder)


if __name__ == "__main__":
    unittest.main()
