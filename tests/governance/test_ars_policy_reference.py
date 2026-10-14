#!/usr/bin/env python3
"""Non-authorizing governance proposal consistency checks (stdlib only).

This oracle MUST fail until every proposed module exists. A passing result is
NOT owner adoption, GitHub rule enforcement, or PR merge authorization.
"""
from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GOV = ROOT / "docs" / "governance"
INDEX = "ARS_POLICY_V0_1_INDEX.md"
MODULES = {
    "ARS_M1_SOLO_OWNER_ACK.md": (526, ("OWNER_DIRECT_ACK", "T0", "T1", "T2", "EXPLICIT_MERGE_ACK")),
    "ARS_M2_DEVELOPER.md": (527, ("GPT_DEVELOPER", "SELF_CHECKS", "FINAL_HANDOFF")),
    "ARS_M3_FINAL_AUDIT.md": (528, ("FINAL_ONLY_AUDIT", "INDEPENDENCE_NOT_AVAILABLE", "OWNER_ACCEPTED_SOLO_AUDIT_EXCEPTION")),
    "ARS_M4_STACKED_PRS.md": (529, ("MAX_STACK_DEPTH=10", "PARENT_FIRST", "FROZEN_HEAD")),
    "ARS_M5_MERGE_HISTORY.md": (530, ("MERGE_COMMIT", "ONE_PR_ONE_ACK", "NO_SQUASH_BY_DEFAULT")),
    "ARS_M6_WAVE_STATE.md": (531, ("LOCAL_WAVE_STATE=NOT_ADOPTED", "ASSISTANT_M6_SUSPENDED", "NO_AUTO_WAVE")),
}
SOURCE_ROOT = "https://github.com/boxs-51/assistant/issues/"
PROPOSAL = "ADOPTION_STATE=PROPOSED_NOT_CANONICAL"

def verify(texts: dict[str, str]):
    required = {INDEX, *MODULES}
    if set(texts) != required:
        raise AssertionError("Missing or extra policy proposal module(s)")
    index = texts[INDEX]
    for token in (PROPOSAL, "ARS_POLICY85_V3_SELECTIVE_REFERENCE_ACK=YES",
                  "REFERENCE_VERSION=assistant-policy-85-v3.0",
                  "NO_AUTOMATIC_INHERITANCE", "INDEPENDENCE_NOT_AVAILABLE",
                  "MERGE_NOT_AUTHORIZED", "M1_NOT_AUTHORIZED",
                  "OWNER_G00_FREEZE_SHA=fe3e680a44ac4e71ef57dc177e73ae31198fcbd0",
                  "PR11_PARENT_SHA=419a94bd567986ab5edd8ae5f4f11e6fae8ee4fb",
                  "PR15_MAIN_INTEGRATION=MERGE_HOLD",
                  "https://github.com/boxs-51/assistant/issues/85",
                  "GITHUB_PROTECTION_VERIFICATION=HOLD"):
        if token not in index:
            raise AssertionError("Index lost scope/owner/platform boundary: " + token)
    for fn, (source, markers) in MODULES.items():
        doc = texts[fn]
        if PROPOSAL not in doc:
            raise AssertionError("Proposal claimed active without main merge: " + fn)
        if SOURCE_ROOT + str(source) not in doc:
            raise AssertionError("Mismatched assistant v3 module source: " + fn)
        for marker in markers:
            if marker not in doc:
                raise AssertionError("Missing module contract " + fn + ": " + marker)
        if f"docs/governance/{fn}" not in index:
            raise AssertionError("Index not mapping exact governance module: " + fn)
    if "LOCAL_WAVE_STATE=ACTIVE" in texts[INDEX] or any(
            "LOCAL_WAVE_STATE=ACTIVE" in doc for doc in texts.values()):
        raise AssertionError("An unapproved Avatar wave cannot be ACTIVE")
    if "AUTO_MERGE=YES" in "\n".join(texts.values()):
        raise AssertionError("No policy draft creates merge permission")
    if "ACTOR_DISTINCT_GITHUB_REVIEW=PASS" in "\n".join(texts.values()):
        raise AssertionError("Cannot fabricate a separate human reviewer")
    return True

def files():
    return {name: (GOV / name).read_text(encoding="utf-8")
            for name in (INDEX, *MODULES)}

class ARSPolicyReferenceProposal(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.texts = files()
    def test_exact_modules_and_reference_boundary(self):
        self.assertTrue(verify(self.texts))
    def test_incomplete_module_fails(self):
        text = dict(self.texts)
        del text["ARS_M4_STACKED_PRS.md"]
        with self.assertRaises(AssertionError): verify(text)
    def test_wrong_upstream_module_fails(self):
        text = dict(self.texts)
        p = "ARS_M5_MERGE_HISTORY.md"
        text[p] = text[p].replace(SOURCE_ROOT + "530", SOURCE_ROOT + "531")
        with self.assertRaises(AssertionError): verify(text)
    def test_implicit_wave_activation_fails(self):
        text = dict(self.texts)
        text["ARS_M6_WAVE_STATE.md"] += "\nLOCAL_WAVE_STATE=ACTIVE"
        with self.assertRaises(AssertionError): verify(text)
    def test_hidden_auto_merge_fails(self):
        text = dict(self.texts)
        text[INDEX] += "\nAUTO_MERGE=YES"
        with self.assertRaises(AssertionError): verify(text)
    def test_fake_reviewer_fails(self):
        text = dict(self.texts)
        text["ARS_M3_FINAL_AUDIT.md"] += "\nACTOR_DISTINCT_GITHUB_REVIEW=PASS"
        with self.assertRaises(AssertionError): verify(text)
    def test_reference_yes_not_adoption_yes(self):
        text = dict(self.texts)
        text[INDEX] = text[INDEX].replace(PROPOSAL, "ADOPTION_STATE=ACTIVE")
        with self.assertRaises(AssertionError): verify(text)

if __name__ == "__main__":
    unittest.main()
