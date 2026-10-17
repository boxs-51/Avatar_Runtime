"""M2-A may only extend the held M1-D head with private 2D preparation."""
import pathlib
import subprocess
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins

ROOT = pathlib.Path(__file__).resolve().parents[2]


class Custody(unittest.TestCase):
    def test_main_parent_contains_exact_m1_d_merge(self):
        self.assertEqual(pins.git("rev-parse", pins.M2_A_MAIN_PARENT + "^1"),
                         "f5b198137f6a96a2e33bb8ac206e53c3815bbbbc")
        self.assertEqual(pins.git("rev-parse", pins.M2_A_MAIN_PARENT + "^2"),
                         pins.M2_A_BASE)

    def test_exact_m2_scope(self):
        self.assertTrue(pins.check_m2_a(pins.M2_A_BASE, "HEAD"))

    def test_wrong_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_a(pins.M1_D_BASE, "HEAD")

    def test_extra_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_A_BASE, "HEAD"):
                return result + "\nsrc/abi/unreviewed.cpp"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_a(pins.M2_A_BASE, "HEAD")

    def test_public_abi_still_unsupported(self):
        source = (ROOT / "src/abi/unsupported.cpp").read_text(encoding="utf-8")
        self.assertIn("ars_runtime_render", source)


if __name__ == "__main__":
    unittest.main()
