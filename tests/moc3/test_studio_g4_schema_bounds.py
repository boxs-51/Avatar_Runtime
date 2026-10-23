"""G4-REV-01 and G4-VAL-02 RED-first oracles; no Native edits."""
import json
import unittest
from scripts.avatar_toolkit_v2 import execute_request


class BoundaryBridge:
    def __init__(self, parent=1, child=2):
        self.parent, self.child = parent, child
        self.ops = []

    def call(self, command):
        op = command["op"]
        self.ops.append(op)
        if op == "studio.epoch":
            return {"ok": True, "receipt": "generation=55"}
        if op == "studio.status":
            return {"ok": True, "receipt":
                    "frame=1 playing=0 expression=0 tab=3 xpbd=0 "
                    "params=1 parts=1 drawables=2"}
        if command.get("generation") != 55 or command.get("drawable") != 0:
            return {"ok": False, "receipt": "denied"}
        if op == "g4.rig.revision":
            return {"ok": True, "receipt": "revision=11"}
        if op == "g4.rig.meta":
            return {"ok": True, "receipt":
                    "drawable=0 vertices=3 role=0 anchors=0 beams=0 "
                    "parents=1 weights=0 cross=0"}
        if op == "g4.rig.entry":
            return {"ok": True, "receipt":
                    f"kind=parent index=0 parent={self.parent} "
                    f"child={self.child} stiffness=.5"}
        raise AssertionError("unexpected Native opcode")


def ask(operation, arguments, bridge=None):
    payload = {
        "api_version": 2, "request_id": "req-g4-bounds",
        "session_id": None, "expected_model_generation":
            None if operation == "avatar.schema" else 55,
        "expected_edit_revision": None,
        "operation": operation, "arguments": arguments, "mode": "read",
    }
    return json.loads(execute_request(json.dumps(payload).encode(),
                 local_bridge=bridge, allow_local_private=True))


class G4SchemaAndBounds(unittest.TestCase):
    def test_rig_schema_advertises_native_revision_guard(self):
        for op in ("rig.local.inspect", "rig.entries.page"):
            with self.subTest(op=op):
                answer = ask("avatar.schema", {"operation": op}, BoundaryBridge())
                self.assertEqual(answer["state"], "COMPLETED_READ")
                self.assertTrue(answer["result"]["editor_revision_fence"])

    def test_mesh_schema_stays_editor_unfenced(self):
        answer = ask("avatar.schema",
                     {"operation": "mesh.vertices.page"}, BoundaryBridge())
        self.assertEqual(answer["state"], "COMPLETED_READ")
        self.assertFalse(answer["result"]["editor_revision_fence"])

    def test_valid_parent_and_child_at_upper_boundary(self):
        response = ask("rig.entries.page",
            {"drawable": 0, "kind": "parent", "start": 0, "limit": 1},
            BoundaryBridge(parent=1, child=2))
        self.assertEqual(response["state"], "COMPLETED_READ")
        self.assertEqual(response["result"]["items"][0]["child"], 2)
        self.assertEqual(response["result"]["editor_revision"], 11)

    def test_invalid_source_parent_and_child_fail_closed(self):
        for parent, child in ((3, 1), (1, 3), (1, 999)):
            with self.subTest(parent=parent, child=child):
                response = ask("rig.entries.page",
                    {"drawable": 0, "kind": "parent", "start": 0, "limit": 1},
                    BoundaryBridge(parent=parent, child=child))
                self.assertEqual(response["state"], "REJECTED")
                self.assertEqual(response["error"]["code"], "INVALID_DRAFT")
                self.assertIsNone(response["result"])


if __name__ == "__main__":
    unittest.main()
