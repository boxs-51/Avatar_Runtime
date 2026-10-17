"""M2-A may only extend the held M1-D head with private 2D preparation."""
import pathlib
import subprocess
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins

ROOT = pathlib.Path(__file__).resolve().parents[2]
CANDIDATE = "10f542f7acb10527bace0dca6d7c608b20018896"


class Custody(unittest.TestCase):
    def test_exact_m2_scope(self):
        subprocess.run(["git", "merge-base", "--is-ancestor", CANDIDATE, "HEAD"],
                       cwd=ROOT, check=True)
        self.assertTrue(pins.check_m2_a(pins.M2_A_BASE, CANDIDATE))

    def test_wrong_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_a(pins.M1_D_BASE, CANDIDATE)

    def test_extra_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_A_BASE, CANDIDATE):
                return result + "\nsrc/abi/unreviewed.cpp"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_a(pins.M2_A_BASE, CANDIDATE)

    def test_public_abi_still_unsupported(self):
        source = (ROOT / "src/abi/unsupported.cpp").read_text(encoding="utf-8")
        self.assertIn("ars_runtime_render", source)


if __name__ == "__main__":
    unittest.main()
