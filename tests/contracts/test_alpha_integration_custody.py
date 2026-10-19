"""Current alpha ancestry and protected blobs; historical allowlists stay exact."""
import subprocess
import os
import contextlib
import io
import unittest
from unittest.mock import patch
import check_alpha_custody as alpha
import check_m0_rc1_pins as pins

class AlphaCustody(unittest.TestCase):
    def test_preserved_integration_and_current_boundaries(self):
        self.assertTrue(alpha.check_alpha())

    def test_wrong_main_parent_rejected(self):
        with self.assertRaises(AssertionError):
            alpha.check_alpha(base="0" * 40)

    def test_candidate_without_integration_rejected(self):
        with self.assertRaises(subprocess.CalledProcessError):
            alpha.check_alpha(candidate=alpha.MAIN)

    def test_altered_source_order_or_parent_rejected(self):
        for suffix in ("^1", "^2"):
            real = pins.git
            def altered(*args):
                if args[0] == "rev-parse" and args[1].endswith(suffix):
                    return "0" * 40
                return real(*args)
            with self.subTest(suffix=suffix), patch.object(pins, "git", side_effect=altered):
                with self.assertRaises(AssertionError):
                    alpha.check_alpha()

class AlphaPullRequestRouting(unittest.TestCase):
    BRANCH = "fix/alpha-52-native-test-targets"
    BASE = "a17d4f92d246ad189b569527b05b471f076eb8b2"
    SOURCE = "ea80850977ae26316572b171795dd10e1b607d7f"

    def run_context(self, base=None, source=None, branch=None, base_ref="alpha", event="pull_request", alteration=None):
        base=self.BASE if base is None else base;source=self.SOURCE if source is None else source
        branch=self.BRANCH if branch is None else branch;real=pins.git
        def merge_view(*args):
            if args == ("rev-parse", "HEAD^1"):return base
            if args == ("rev-parse", "HEAD^2"):return source
            if alteration is not None:
                override=alteration(args)
                if override is not None:return override
            return real(*args)
        env={"GITHUB_EVENT_NAME":event,"GITHUB_BASE_REF":base_ref,"GITHUB_HEAD_REF":branch}
        with patch.dict(os.environ,env),patch.object(pins,"git",side_effect=merge_view),contextlib.redirect_stdout(io.StringIO()):
            pins.check()

    def test_actual_issue52_source_dispatch(self):
        self.run_context()

    def test_issue53_route_fixture_and_shared_checks(self):
        # Synthetic scope fixture only; actual committed53 scope is checked at delivery.
        branch="fix/alpha-53-historical-custody";base,paths=alpha.PR_SCOPES[branch]
        def fixture(args):
            if args == ("diff","--name-only",base,base):return "\n".join(sorted(paths))
        self.run_context(base=base,source=base,branch=branch,alteration=fixture)

    def test_unknown_alpha_branch_rejected(self):
        with self.assertRaises(AssertionError):
            self.run_context(branch="fix/alpha-unregistered")

    def test_missing_or_wrong_base_ref_cannot_select_alpha_route(self):
        for ref in ("", "main"):
            with self.subTest(ref=ref),self.assertRaises(AssertionError):
                self.run_context(base_ref=ref)

    def test_moved_issue_base_rejected(self):
        with self.assertRaises(AssertionError):
            self.run_context(base="0"*40)

    def test_extra_issue_path_rejected(self):
        paths=alpha.PR_SCOPES[self.BRANCH][1]
        def extra(args):
            if args == ("diff","--name-only",self.BASE,self.SOURCE):
                return "\n".join(sorted(paths))+"\nsrc/unreviewed.cpp"
        with self.assertRaises(AssertionError):
            self.run_context(alteration=extra)

    def test_candidate_blob_checked_independently_from_checkout(self):
        def corrupt(args):
            if args == ("rev-parse",self.SOURCE+":include/ars/ars.h"):return "0"*40
        with self.assertRaises(AssertionError):
            self.run_context(alteration=corrupt)

    def test_non_pr_event_does_not_select_pr_route(self):
        with patch.object(alpha,"check_alpha_pull_request",side_effect=AssertionError("PR route used on push")):
            self.run_context(event="push")

    def test_legacy_m1_a_route_remains_strict_and_supported(self):
        self.run_context(base=pins.M1_A_BASE,source="b1d634036e538f0866c3be77b85d2de890b85c81",
                         branch=pins.M1_A_BRANCH,base_ref="main")

    def test_extra_frozen_integration_path_rejected(self):
        real = pins.git
        def altered(*args):
            result = real(*args)
            if args == ("diff", "--name-only", alpha.MOC3_BASE, alpha.INTEGRATION):
                return result + "\nsrc/unreviewed.cpp"
            return result
        with patch.object(pins, "git", side_effect=altered):
            with self.assertRaises(AssertionError):
                alpha.check_alpha()

    def test_changed_public_or_hold_blob_rejected(self):
        for path in alpha.HELD_PATHS:
            real = pins.git
            def altered(*args):
                if args == ("rev-parse", "HEAD:" + path):
                    return "0" * 40
                return real(*args)
            with self.subTest(path=path), patch.object(pins, "git", side_effect=altered):
                with self.assertRaises(AssertionError):
                    alpha.check_alpha()
