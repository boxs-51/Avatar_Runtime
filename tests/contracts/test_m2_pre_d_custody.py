"""Pre-M2-D 3D evidence remains on the exact design parent and private paths."""
import unittest
import pathlib
import subprocess
from unittest.mock import patch

import check_m0_rc1_pins as pins
ROOT = pathlib.Path(__file__).resolve().parents[2]
CANDIDATE = "d96ff59ba0642308f0b656709fbccb511d142a93"


class Custody(unittest.TestCase):
    def test_exact_scope(self):
        subprocess.run(["git", "merge-base", "--is-ancestor", CANDIDATE, "HEAD"],
                       cwd=ROOT, check=True)
        self.assertTrue(pins.check_m2_pre_d(pins.M2_PRE_D_BASE, CANDIDATE))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_pre_d(pins.M2_D_DESIGN_BASE, CANDIDATE)

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_PRE_D_BASE, CANDIDATE):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_pre_d(pins.M2_PRE_D_BASE, CANDIDATE)
