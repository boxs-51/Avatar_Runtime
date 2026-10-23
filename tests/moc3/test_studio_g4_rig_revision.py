"""RED-first for a Native owner-thread Rig revision fence, including ABA."""
import json
import unittest

from scripts.avatar_toolkit_v2 import execute_request
from scripts.studio_agent_api import validate_command


class RigRevisionFixture:
    def __init__(self, *, change=False, malformed=False):
        self.calls = []
        self.change = change
        self.malformed = malformed
        self.rev_reads = 0

    def call(self, command):
        self.calls.append(command)
        op = command["op"]
        if op == "studio.epoch":
            return {"ok": True, "receipt": "generation=55"}
        if op == "studio.status":
            return {"ok": True, "receipt":
                    "frame=10 playing=0 expression=0 tab=3 xpbd=0 "
                    "params=2 parts=1 drawables=1"}
        if command.get("generation") != 55 or command.get("drawable") != 0:
            return {"ok": False, "receipt": "rejected"}
        if op == "g4.rig.revision":
            self.rev_reads += 1
            if self.malformed:
                return {"ok": True, "receipt": "revision=-1"}
            current = 3 if self.change and self.rev_reads > 1 else 1
            return {"ok": True, "receipt": f"revision={current}"}
        if op == "g4.rig.meta":
            return {"ok": True, "receipt":
                    "drawable=0 vertices=3 role=0 anchors=2 beams=0 "
                    "parents=0 weights=0 cross=0"}
        if op == "g4.rig.entry":
            index = command["index"]
            return {"ok": True, "receipt":
                    f"kind=anchor index={index} vertex={index}"}
        raise AssertionError(f"unexpected opcode {op}")


def run(bridge):
    payload = {"api_version": 2, "request_id": "req-rig-revision",
               "session_id": None, "expected_model_generation": 55,
               "expected_edit_revision": None, "operation": "rig.entries.page",
               "arguments": {"drawable": 0, "kind": "anchor",
                             "start": 0, "limit": 2}, "mode": "read"}
    return json.loads(execute_request(json.dumps(payload).encode("utf-8"),
                                       local_bridge=bridge,
                                       allow_local_private=True))


class RigRevisionTests(unittest.TestCase):
    def test_strict_native_revision_opcode(self):
        self.assertEqual(
            validate_command({"op": "g4.rig.revision",
                              "generation": 55, "drawable": 0}),
            "G4_RIG_REV 55 0")
        for invalid in (
            {"op": "g4.rig.revision", "generation": True, "drawable": 0},
            {"op": "g4.rig.revision", "generation": 55, "drawable": -1},
            {"op": "g4.rig.revision", "generation": 55, "drawable": 0,
             "extra": 1},
        ):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                validate_command(invalid)

    def test_no_edit_passes_and_reports_fence(self):
        bridge = RigRevisionFixture()
        response = run(bridge)
        self.assertEqual(response["state"], "COMPLETED_READ")
        self.assertEqual([item["vertex"] for item in
                          response["result"]["items"]], [0, 1])
        self.assertTrue(response["result"]["editor_revision_available"])
        self.assertEqual(response["result"]["editor_revision"], 1)
        self.assertEqual(bridge.rev_reads, 2)

    def test_aba_change_must_reject_entire_page(self):
        bridge = RigRevisionFixture(change=True)
        response = run(bridge)
        self.assertEqual(response["state"], "REJECTED")
        self.assertEqual(response["error"]["code"], "STALE_REVISION")
        self.assertIsNone(response["result"])
        self.assertEqual(bridge.rev_reads, 2)

    def test_malformed_revision_fails_closed(self):
        bridge = RigRevisionFixture(malformed=True)
        response = run(bridge)
        self.assertEqual(response["state"], "REJECTED")
        self.assertIsNone(response["result"])


if __name__ == "__main__":
    unittest.main()
