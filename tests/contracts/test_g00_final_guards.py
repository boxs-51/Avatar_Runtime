"""RED-first guards: immutable provenance path set and pinned actual G00 blobs."""
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/contracts"))
import check_m0_rc1_pins as pins

class G00PinFailClosed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = json.loads((ROOT / "docs/M0_CONTRACT_PARENT_PINS.json").read_text("utf-8"))

    def test_original_complete_and_canonical(self):
        self.assertTrue(pins.validate_blob_manifest(self.doc))
    def test_one_removed_predecessor_path_rejected(self):
        for victim in list(self.doc["source_blobs"]):
            with self.subTest(victim=victim):
                copy = json.loads(json.dumps(self.doc))
                del copy["source_blobs"][victim]
                with self.assertRaises(AssertionError):
                    pins.validate_blob_manifest(copy)
    def test_extra_predecessor_path_rejected(self):
        copy=json.loads(json.dumps(self.doc))
        copy["source_blobs"]["docs/alien.md"]="a"*40
        with self.assertRaises(AssertionError):
            pins.validate_blob_manifest(copy)
    def test_actual_public_abi_must_be_pinned(self):
        copy=json.loads(json.dumps(self.doc))
        del copy["reviewed_g00_blobs"]["include/ars/ars.h"]
        with self.assertRaises(AssertionError):
            pins.validate_blob_manifest(copy)
    def test_actual_graphics_contract_must_be_pinned(self):
        copy=json.loads(json.dumps(self.doc))
        del copy["reviewed_g00_blobs"]["docs/ADR_0001_D3D11_G00_DESIGN_RC1.md"]
        with self.assertRaises(AssertionError):
            pins.validate_blob_manifest(copy)
    def test_source_blob_sha_incorrect_rejected(self):
        copy=json.loads(json.dumps(self.doc))
        copy["source_blobs"]["schemas/character_manifest_v0.1.schema.json"]="0"*40
        with self.assertRaises(AssertionError):
            pins.validate_blob_manifest(copy)

if __name__ == "__main__":
    unittest.main()
