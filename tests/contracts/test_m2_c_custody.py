"""M2-C implementation stays on the exact design head and private paths."""
import unittest
import pathlib
import subprocess
from unittest.mock import patch

import check_m0_rc1_pins as pins

ROOT = pathlib.Path(__file__).resolve().parents[2]
M2_C_IMPLEMENTATION = "710b3504eb6e1619bf75842082d1fc35df225249"


class Custody(unittest.TestCase):
    def test_exact_m2_c_scope(self):
        subprocess.run(["git", "merge-base", "--is-ancestor", M2_C_IMPLEMENTATION, "HEAD"],
                       cwd=ROOT, check=True)
        self.assertTrue(pins.check_m2_c(pins.M2_C_BASE, M2_C_IMPLEMENTATION))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_c(pins.M2_C_PARENT, M2_C_IMPLEMENTATION)

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_C_BASE, M2_C_IMPLEMENTATION):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_c(pins.M2_C_BASE, M2_C_IMPLEMENTATION)


if __name__ == "__main__":
    unittest.main()
