"""M2-D source and evidence stay on the pinned pre-D private branch."""
import unittest
from unittest.mock import patch

import check_m0_rc1_pins as pins


class Custody(unittest.TestCase):
    def test_complete_stack_against_merged_m2_b(self):
        self.assertTrue(pins.check_m2_d_stack_main(pins.M2_D_STACK_MAIN_PARENT, "HEAD"))

    def test_stack_rejects_moved_main_parent(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_d_stack_main(pins.M2_A_MAIN_PARENT, "HEAD")

    def test_exact_scope(self):
        self.assertTrue(pins.check_m2_d_impl(pins.M2_D_IMPL_BASE, "HEAD"))

    def test_moved_parent_rejected(self):
        with self.assertRaises(AssertionError):
            pins.check_m2_d_impl(pins.M2_PRE_D_BASE, "HEAD")

    def test_public_path_rejected(self):
        real = pins.git

        def changed(*args):
            result = real(*args)
            if args == ("diff", "--name-only", pins.M2_D_IMPL_BASE, "HEAD"):
                return result + "\ninclude/ars/ars.h"
            return result

        with patch.object(pins, "git", side_effect=changed):
            with self.assertRaises(AssertionError):
                pins.check_m2_d_impl(pins.M2_D_IMPL_BASE, "HEAD")
