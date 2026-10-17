"""M2-B offscreen pixels stay on the exact M2-A parent and private paths."""
import unittest
import pathlib
import subprocess
from unittest.mock import patch

import check_m0_rc1_pins as pins
ROOT = pathlib.Path(__file__).resolve().parents[2]
CANDIDATE = "6e3e013c8211a7a2c20ea3520ca2a5be599f79fb"


class Custody(unittest.TestCase):
    def test_exact_m2_b_scope(self):
        subprocess.run(["git", "merge-base", "--is-ancestor", CANDIDATE, "HEAD"],
                       cwd=ROOT, check=True)
        self.assertTrue(pins.check_m2_b(pins.M2_B_BASE, CANDIDATE))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_b(pins.M2_A_BASE, CANDIDATE)

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_B_BASE, CANDIDATE):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_b(pins.M2_B_BASE, CANDIDATE)


if __name__ == "__main__":
    unittest.main()
