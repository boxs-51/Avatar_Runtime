"""M2-D planning stays on the exact M2-C implementation head."""
import unittest
import pathlib
import subprocess
from unittest.mock import patch

import check_m0_rc1_pins as pins
ROOT = pathlib.Path(__file__).resolve().parents[2]
CANDIDATE = "8ec80213e8b93f15d5254f8f523ba596000e1c30"


class Custody(unittest.TestCase):
    def test_exact_scope(self):
        subprocess.run(["git", "merge-base", "--is-ancestor", CANDIDATE, "HEAD"],
                       cwd=ROOT, check=True)
        self.assertTrue(pins.check_m2_d_design(pins.M2_D_DESIGN_BASE, CANDIDATE))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_d_design(pins.M2_C_BASE, CANDIDATE)

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_D_DESIGN_BASE, CANDIDATE):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_d_design(pins.M2_D_DESIGN_BASE, CANDIDATE)
