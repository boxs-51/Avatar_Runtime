"""M2-C implementation stays on the exact design head and private paths."""
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins


class Custody(unittest.TestCase):
    def test_exact_m2_c_scope(self):
        self.assertTrue(pins.check_m2_c(pins.M2_C_BASE, "HEAD"))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_c(pins.M2_C_PARENT, "HEAD")

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_C_BASE, "HEAD"):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_c(pins.M2_C_BASE, "HEAD")


if __name__ == "__main__":
    unittest.main()
