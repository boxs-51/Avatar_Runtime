"""M2-D planning stays on the exact M2-C implementation head."""
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins


class Custody(unittest.TestCase):
    def test_exact_scope(self):
        self.assertTrue(pins.check_m2_d_design(pins.M2_D_DESIGN_BASE, "HEAD"))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_d_design(pins.M2_C_BASE, "HEAD")

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_D_DESIGN_BASE, "HEAD"):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_d_design(pins.M2_D_DESIGN_BASE, "HEAD")
