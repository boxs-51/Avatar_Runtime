#!/usr/bin/env python3
"""P2-04 RED-first G00 canonical specification check (design only)."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
S = ROOT / 'docs/reference/SPECIFICATION_V0.1.md'
HEADER = ROOT / "include/ars/ars.h"
LIST_START = "<!-- G00_CANONICAL_ABI20_START -->"
LIST_END = "<!-- G00_CANONICAL_ABI20_END -->"

def check_canonical(spec, header):
    signatures = re.findall(
        r"ARS_API\s+\w+\s+ARS_CALL\s+(ars_[a-z_0-9]+)\s*\(", header)
    if len(signatures)!=20 or len(set(signatures))!=20:
        raise AssertionError("Public C ABI must expose exactly twenty unique declarations")
    start = spec.index(LIST_START)
    end = spec.index(LIST_END, start)
    if end <= start:
        raise AssertionError("Missing canonical ABI20 list")
    listed = re.findall(r"\x60(ars_[a-z_0-9]+)\x60",spec[start:end])
    if len(listed)!=20 or len(set(listed))!=20 or set(listed)!=set(signatures):
        raise AssertionError("Spec canonical ABI20 differs from compiled public C header")
    contract = spec[spec.index("## 5. Render API and ABI contract"):
                    spec.index("## 6. Motion and animation pipeline")]
    prohibited = ("typedef struct ars_runtime_t ars_runtime_t;",
                  "ars_runtime_destroy(", "ars_get_capabilities(",
                  "ars_runtime_t**", "ars_result_t ars_motion_submit(",
                  "Host/device ownership modes: **HOST_DEVICE**")
    for token in prohibited:
        if token in contract:
            raise AssertionError("Legacy pointer/dual-device API sketch remains: "+token)
    required = ("ars_handle_id", "runtime_uid", "slot", "generation",
                "ars_status", "int32_t", "ARS_ABI_MAJOR", "struct_size",
                "abi_version", "ARS_CAP_D3D11_HOST_DEVICE",
                "ARS_CAP_HOST_SCOPED_MOTION_INGRESS",
                "D3D11_QUERY_EVENT", "HOST_DEVICE-only",
                "SDK_DEVICE is deferred", "M1-D", "G00", "G01", "G10",
                "ARS_E_UNSUPPORTED", "ARS_E_BAD_ASSET",
                "ARS-SOLO-OWNER-001", "NO MERGE",
                "source authority", "caller-owned", "COM")
    for token in required:
        if token not in contract:
            raise AssertionError("Missing normative G00 contract: "+token)
    if "SDK_DEVICE creates a private device" in contract:
        raise AssertionError("SDK_DEVICE incorrectly advertised as baseline")
    return True

class CanonicalSpec(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spec=S.read_text(encoding="utf-8")
        cls.header=HEADER.read_text(encoding="utf-8")
    def test_exact_current_abi20_and_host_device(self):
        self.assertTrue(check_canonical(self.spec,self.header))
    def test_old_pointer_sketch_must_fail(self):
        marker="typedef struct ars_runtime_t ars_runtime_t;"
        modified=self.spec.replace("## 6. Motion and animation pipeline",
                                   marker+"\n\n## 6. Motion and animation pipeline")
        with self.assertRaises((AssertionError,ValueError)):
            check_canonical(modified,self.header)
    def test_missing_public_symbol_must_fail(self):
        with self.assertRaises((AssertionError,ValueError)):
            check_canonical(self.spec.replace(chr(96)+"ars_runtime_finish_close"+chr(96), "missing"),
                            self.header)
    def test_sdk_device_advertisement_must_fail(self):
        with self.assertRaises((AssertionError,ValueError)):
            check_canonical(self.spec.replace("SDK_DEVICE is deferred",
                                               "SDK_DEVICE creates a private device"),
                            self.header)

if __name__=="__main__":
    unittest.main()
