"""RED-first G4-VAL-03/G4-SCHEMA-04: source-vs-target indices, exact schemas."""
import json
import unittest

from scripts.avatar_toolkit_v2 import execute_request


class LocalFixture:
    def __init__(self, kind="cross", parent=1, child=4, target=1,
                 target_vertices=5, target_receipt_drawable=1, target_available=True):
        self.kind, self.parent, self.child, self.target = kind, parent, child, target
        self.target_vertices = target_vertices
        self.target_receipt_drawable = target_receipt_drawable
        self.target_available = target_available
        self.calls = []

    def call(self, command):
        self.calls.append(dict(command))
        op = command["op"]
        if op == "studio.epoch":
            return {"ok": True, "receipt": "generation=55"}
        if op == "studio.status":
            return {"ok": True, "receipt":
                    "frame=1 playing=0 expression=0 tab=3 xpbd=0 params=1 parts=1 drawables=2"}
        if command.get("generation") != 55:
            return {"ok": False, "receipt": "stale"}
        if op == "g4.rig.revision":
            return {"ok": True, "receipt": "revision=7"}
        if op == "g4.rig.meta":
            return {"ok": True, "receipt":
                    "drawable=0 vertices=3 role=0 anchors=0 beams=0 parents=1 weights=0 cross=1"}
        if op == "g4.rig.entry":
            if self.kind == "parent":
                return {"ok": True, "receipt":
                        f"kind=parent index=0 parent={self.parent} child={self.child} stiffness=.5"}
            return {"ok": True, "receipt":
                    f"kind=cross index=0 parent={self.parent} target={self.target} "
                    f"child={self.child} stiffness=.5"}
        if op == "g4.mesh.vertex_count" and command.get("drawable") == 1 and self.target_available:
            return {"ok": True, "receipt":
                    f"drawable={self.target_receipt_drawable} vertices={self.target_vertices}"}
        return {"ok": False, "receipt": "invalid"}


def call(op, args, bridge=None):
    envelope = {
        "api_version": 2, "request_id": "req-g4-relation-validation",
        "session_id": None, "expected_model_generation":
            None if op == "avatar.schema" else 55,
        "expected_edit_revision": None, "operation": op,
        "arguments": args, "mode": "read",
    }
    return json.loads(execute_request(json.dumps(envelope).encode("utf-8"),
                      local_bridge=bridge, allow_local_private=True))


def page(bridge, kind):
    return call("rig.entries.page", {"drawable": 0, "kind": kind,
                                     "start": 0, "limit": 1}, bridge)


class RelationsRED(unittest.TestCase):
    def assertDenied(self, response):
        self.assertEqual(response["state"], "REJECTED")
        self.assertEqual(response["error"]["code"], "INVALID_DRAFT")
        self.assertIsNone(response["result"])

    def test_parent_self_reference_rejected(self):
        self.assertDenied(page(LocalFixture(kind="parent", parent=1, child=1), "parent"))

    def test_cross_invalid_target_and_source_target_rejected(self):
        for target in (0, 2, 4095):
            with self.subTest(target=target):
                bridge = LocalFixture(target=target)
                self.assertDenied(page(bridge, "cross"))
                self.assertFalse(any(c["op"] == "g4.mesh.vertex_count" for c in bridge.calls))

    def test_cross_target_child_must_fit_target_mesh(self):
        for child in (5, 4095):
            with self.subTest(child=child):
                self.assertDenied(page(LocalFixture(child=child), "cross"))

    def test_cross_target_meta_spoof_or_unavailable_denied(self):
        spoofed = page(LocalFixture(target_receipt_drawable=0), "cross")
        self.assertEqual(spoofed["state"], "REJECTED")
        self.assertEqual(spoofed["error"]["code"], "STALE_MODEL")
        self.assertIsNone(spoofed["result"])
        self.assertDenied(page(LocalFixture(target_available=False), "cross"))

    def test_cross_valid_target_child_not_source_bounded(self):
        bridge = LocalFixture(child=4, target_vertices=5)
        r = page(bridge, "cross")
        self.assertEqual(r["state"], "COMPLETED_READ")
        self.assertEqual(r["result"]["items"][0]["child"], 4)
        self.assertEqual(r["result"]["editor_revision"], 7)
        self.assertEqual([c["drawable"] for c in bridge.calls if c["op"] == "g4.mesh.vertex_count"], [1])

    def test_valid_nonself_parent_preserved(self):
        r = page(LocalFixture(kind="parent", parent=1, child=2), "parent")
        self.assertEqual(r["state"], "COMPLETED_READ")

    def test_mesh_schemas_do_not_advertise_kind(self):
        for op in ("mesh.vertices.page", "mesh.triangles.page"):
            with self.subTest(op=op):
                schema = call("avatar.schema", {"operation": op}, LocalFixture())
                self.assertEqual(schema["state"], "COMPLETED_READ")
                self.assertEqual(set(schema["result"]["arguments"]),
                                 {"drawable", "start", "limit"})

    def test_rig_schema_requires_kind(self):
        schema = call("avatar.schema", {"operation": "rig.entries.page"}, LocalFixture())
        self.assertEqual(schema["state"], "COMPLETED_READ")
        self.assertEqual(set(schema["result"]["arguments"]),
                         {"drawable", "start", "limit", "kind"})


if __name__ == "__main__":
    unittest.main()
