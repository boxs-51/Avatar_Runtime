"""M1-D keeps the frozen public ABI and prior security/design authorities."""
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
BASE = "f5b198137f6a96a2e33bb8ac206e53c3815bbbbc"
FROZEN = (
    "include/ars/ars.h",
    "src/abi/unsupported.cpp",
    "docs/ADR_0001_D3D11_G00_DESIGN_RC1.md",
    "docs/G00_SECURITY_87_CASE_INVENTORY.json",
    "docs/M0_CONTRACT_PARENT_PINS.json",
)


class Custody(unittest.TestCase):
    def test_frozen_blobs_match_merged_c2(self):
        for name in FROZEN:
            with self.subTest(path=name):
                historical = subprocess.run(
                    ["git", "show", f"{BASE}:{name}"], cwd=ROOT,
                    capture_output=True, check=True,
                ).stdout
                checked_out = (ROOT / name).read_bytes().replace(b"\r\n", b"\n")
                self.assertEqual(checked_out, historical)

    def test_public_render_stays_unsupported(self):
        source = (ROOT / "src/abi/unsupported.cpp").read_text(encoding="utf-8")
        self.assertIn("ars_runtime_render", source)
        self.assertIn("ars_runtime_drain_gpu", source)
        # The frozen file comparison above preserves their actual behavior.


if __name__ == "__main__":
    unittest.main()
