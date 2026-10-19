"""The private teardown receipt stays on #37 and leaves public gates held."""
import unittest
import subprocess
from unittest.mock import patch

import check_m0_rc1_pins as pins

CANDIDATE = "59f4a08006733c8e4fe61c4f899d77c150656b1c"


class Custody(unittest.TestCase):
    def test_exact_source_and_scope(self):
        subprocess.run(["git", "merge-base", "--is-ancestor", CANDIDATE, "HEAD"],
                       cwd=pins.ROOT, check=True)
        self.assertTrue(pins.check_m2_e(pins.M2_E_BASE, CANDIDATE))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_e(pins.M2_D_IMPL_BASE, CANDIDATE)

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_E_BASE, CANDIDATE):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_e(pins.M2_E_BASE, CANDIDATE)
