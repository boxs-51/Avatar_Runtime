"""G5 staged CAS editor RED-first contract. Opt-in; no production writes.

Invoke explicitly: python -m pytest -q tests/moc3/g5_redfirst_contract.py
This file is intentionally outside default test_*.py collection.
"""
import json
import unittest

from scripts.avatar_toolkit_v2 import execute_request


class LocalReadProbe:
    """Records all calls; never performs Native editing or touches model files."""
    def __init__(self):
        self.calls = []

    def call(self, command):
        self.calls.append(command)
        op = command.get("op")
        if op == "studio.epoch":
            return {"ok": True, "receipt": "generation=55"}
        if op == "studio.status":
            return {"ok": True, "receipt":
                    "frame=0 playing=0 expression=0 tab=3 xpbd=0 "
                    "params=1 parts=1 drawables=1"}
        if op == "g4.rig.revision":
            return {"ok": True, "receipt": "revision=7"}
        return {"ok": False, "receipt": "rejected"}


def call(operation, arguments=None, *, revision=7, generation=55,
         mode="stage", session_id="session-g5-synthetic", bridge=None):
    envelope = {
        "api_version": 2, "request_id": "req-g5-t1",
        "session_id": session_id,
        "expected_model_generation": generation,
        "expected_edit_revision": revision,
        "operation": operation, "arguments": arguments or {},
        "mode": mode,
    }
    return json.loads(execute_request(json.dumps(envelope).encode("utf-8"),
             local_bridge=bridge, allow_local_private=bridge is not None))


class G5T1Contract(unittest.TestCase):
    def test_control_discovery_does_not_claim_edit_authority(self):
        reply = call("avatar.capabilities", mode="read", session_id=None,
                     generation=None, revision=None)
        self.assertEqual(reply["state"], "COMPLETED_READ")
        self.assertFalse(reply["result"]["mutating_commands_enabled"])

    def test_control_native_mutation_denied_before_g5_gate(self):
        bridge = LocalReadProbe()
        reply = call("draft.begin", {"drawable": 0}, bridge=bridge)
        self.assertEqual(reply["state"], "REJECTED")
        self.assertIsNone(reply["result"])
        self.assertEqual(bridge.calls, [])

    def test_red_exact_cas_can_open_private_draft_without_live_edit(self):
        bridge = LocalReadProbe()
        reply = call("draft.begin", {"drawable": 0}, bridge=bridge)
        self.assertEqual(reply["state"], "STAGED")
        self.assertEqual(reply["result"]["base_revision"], 7)
        self.assertIsInstance(reply["result"]["draft_id"], str)
        self.assertFalse(reply["result"].get("applied", False))

    def test_red_stale_edit_revision_is_conflict_not_unsupported(self):
        bridge = LocalReadProbe()
        reply = call("draft.begin", {"drawable": 0}, revision=6, bridge=bridge)
        self.assertEqual(reply["state"], "REJECTED")
        self.assertEqual(reply["error"]["code"], "REVISION_CONFLICT")
        self.assertIsNone(reply["result"])

    def test_red_commit_without_validated_draft_is_invalid(self):
        bridge = LocalReadProbe()
        reply = call("draft.commit", {"draft_id": "draft-synthetic-unowned"},
                     mode="edit", bridge=bridge)
        self.assertEqual(reply["state"], "REJECTED")
        self.assertEqual(reply["error"]["code"], "INVALID_DRAFT")
        self.assertIsNone(reply["result"])

    def test_red_undo_with_stale_revision_is_conflict(self):
        bridge = LocalReadProbe()
        reply = call("draft.undo", {}, revision=6, mode="edit", bridge=bridge)
        self.assertEqual(reply["state"], "REJECTED")
        self.assertEqual(reply["error"]["code"], "REVISION_CONFLICT")
        self.assertIsNone(reply["result"])


if __name__ == "__main__":
    unittest.main()
