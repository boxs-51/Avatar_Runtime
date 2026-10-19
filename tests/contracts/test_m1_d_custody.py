"""M1-D keeps the frozen public ABI and prior security/design authorities."""
import pathlib
import subprocess
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins

ROOT = pathlib.Path(__file__).resolve().parents[2]
BASE = "f5b198137f6a96a2e33bb8ac206e53c3815bbbbc"
# M2 stacks above this held D head. Keep auditing the exact D candidate,
# while check_m2_a separately constrains all subsequent M2 paths.
CANDIDATE = "dffe10c16d5585c0d50fd7745f310543e719c522"
FROZEN = (
    "include/ars/ars.h",
    "src/abi/unsupported.cpp",
    "docs/ADR_0001_D3D11_G00_DESIGN_RC1.md",
    "docs/G00_SECURITY_87_CASE_INVENTORY.json",
    "docs/M0_CONTRACT_PARENT_PINS.json",
)


class Custody(unittest.TestCase):
    def test_exact_d_branch_scope(self):
        subprocess.run(["git", "merge-base", "--is-ancestor", CANDIDATE, "HEAD"],
                       cwd=ROOT, check=True)
        self.assertTrue(pins.check_m1_d(BASE, CANDIDATE))

    def test_extra_gpu_path_rejected(self):
        real = pins.git
        def added(*args):
            result = real(*args)
            return result + "\nsrc/graphics/unreviewed.cpp" if args == ("diff", "--name-only", BASE, CANDIDATE) else result
        with patch.object(pins, "git", side_effect=added):
            with self.assertRaises(AssertionError): pins.check_m1_d(BASE, CANDIDATE)

    def test_public_abi_mutation_rejected(self):
        real = pins.git
        def changed(*args):
            return "0" * 40 if args == ("rev-parse", CANDIDATE + ":include/ars/ars.h") else real(*args)
        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError): pins.check_m1_d(BASE, CANDIDATE)

    def test_frozen_blobs_match_merged_c2(self):
        for name in FROZEN:
            with self.subTest(path=name):
                historical = subprocess.run(
                    ["git", "show", f"{BASE}:{name}"], cwd=ROOT,
                    capture_output=True, check=True,
                ).stdout
                # Relocation changes only the checkout locator; the historical path,
                # BASE revision and exact byte comparison remain unchanged.
                current = ROOT / "docs/reference" / name.removeprefix("docs/") if name.startswith("docs/") else ROOT / name
                checked_out = current.read_bytes().replace(b"\r\n", b"\n")
                self.assertEqual(checked_out, historical)

    def test_public_render_stays_unsupported(self):
        source = (ROOT / "src/abi/unsupported.cpp").read_text(encoding="utf-8")
        self.assertIn("ars_runtime_render", source)
        self.assertIn("ars_runtime_drain_gpu", source)
        # The frozen file comparison above preserves their actual behavior.


if __name__ == "__main__":
    unittest.main()
