"""RED-first guards: immutable provenance path set and pinned actual G00 blobs."""
import json
import sys
import unittest
import subprocess
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/contracts"))
import check_m0_rc1_pins as pins

class G00PinFailClosed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = json.loads((ROOT / 'docs/reference/M0_CONTRACT_PARENT_PINS.json').read_text("utf-8"))

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

    def test_original_legacy_revision_still_matches(self):
        self.assertTrue(pins.validate_blob_manifest(self.doc, "f2f13614ed0e1f643e5ddb702bf5033f4873768f"))

    def test_alias_does_not_change_manifest_keyset(self):
        copy=json.loads(json.dumps(self.doc));old="docs/P0_OWNER_REMEDIATION_V0.5.md"
        copy["source_blobs"][pins.FROZEN_DOCUMENT_ALIASES[old]]=copy["source_blobs"].pop(old)
        with self.assertRaises(AssertionError):
            pins.validate_blob_manifest(copy)

    def test_missing_divergent_and_wrong_aliases_rejected(self):
        old="docs/P0_OWNER_REMEDIATION_V0.5.md";canonical=pins.FROZEN_DOCUMENT_ALIASES[old]
        expected=self.doc["source_blobs"][old]
        for mode in ("missing", "divergent", "both_wrong", "canonical_wrong"):
            real=pins.git
            def altered(*args):
                if args[0]=="rev-parse" and args[1] in ("HEAD:"+old,"HEAD:"+canonical):
                    if mode=="missing":raise subprocess.CalledProcessError(128,["git",*args])
                    if args[1]=="HEAD:"+canonical:return "0"*40
                    if mode=="canonical_wrong":raise subprocess.CalledProcessError(128,["git",*args])
                    return expected if mode=="divergent" else "0"*40
                return real(*args)
            with self.subTest(mode=mode),patch.object(pins,"git",side_effect=altered):
                with self.assertRaises(AssertionError):
                    pins.validate_blob_manifest(self.doc)

if __name__ == "__main__":
    unittest.main()
