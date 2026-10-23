"""G4 local-only: duplicate identifiers in one catalog page must fail closed."""
import json
import unittest

from scripts.avatar_toolkit_v2 import execute_request


def request(operation, start, limit):
    return json.dumps({
        "api_version": 2,
        "request_id": "req-duplicate-page",
        "session_id": None,
        "expected_model_generation": 100,
        "expected_edit_revision": None,
        "operation": operation,
        "arguments": {"start": start, "limit": limit},
        "mode": "read",
    }).encode("utf-8")


class CatalogFixture:
    def __init__(self, param_ids, part_ids):
        self.params = param_ids
        self.parts = part_ids
        self.calls = []

    def call(self, cmd):
        self.calls.append(cmd)
        op = cmd["op"]
        if op == "studio.epoch":
            return {"ok": True, "receipt": "generation=100"}
        if op == "studio.status":
            return {"ok": True, "receipt": (
                f"frame=1 playing=0 expression=0 tab=0 xpbd=0 "
                f"params={len(self.params)} parts={len(self.parts)} drawables=1")}
        if cmd.get("generation") != 100:
            return {"ok": False, "receipt": "stale"}
        i = cmd["index"]
        if op == "param.inspect.at":
            return {"ok": True, "receipt": f"index={i} value=0 low=-1 high=1 pin=0"}
        if op == "part.inspect.at":
            return {"ok": True, "receipt": f"index={i} opacity=.5"}
        if op in ("param.id.at", "part.id.at"):
            ids = self.params if op.startswith("param.") else self.parts
            return {"ok": True, "receipt": f"index={i} idhex={ids[i].encode('utf-8').hex()}"}
        raise AssertionError("unexpected command")


class DuplicatePageTests(unittest.TestCase):
    def read_page(self, op, start, limit, bridge):
        return json.loads(execute_request(
            request(op, start, limit),
            local_bridge=bridge, allow_local_private=True))

    def test_parameter_duplicate_in_nonzero_page_rejected(self):
        bridge = CatalogFixture(["A", "Duplicate", "Duplicate"], ["part-a", "part-b"])
        response = self.read_page("catalog.parameters.page", 1, 2, bridge)
        self.assertEqual(response["state"], "REJECTED")
        self.assertEqual(response["error"]["code"], "INVALID_DRAFT")
        self.assertIsNone(response["result"])
        self.assertNotIn("Duplicate", json.dumps(response))

    def test_part_duplicate_in_page_rejected(self):
        bridge = CatalogFixture(["A", "B"], ["Same", "Same", "unique"])
        response = self.read_page("catalog.parts.page", 0, 2, bridge)
        self.assertEqual(response["state"], "REJECTED")
        self.assertEqual(response["error"]["code"], "INVALID_DRAFT")
        self.assertIsNone(response["result"])
        self.assertNotIn("Same", json.dumps(response))

    def test_distinct_page_remains_readable(self):
        bridge = CatalogFixture(["A", "B", "C"], ["one", "two"])
        response = self.read_page("catalog.parameters.page", 1, 2, bridge)
        self.assertEqual(response["state"], "COMPLETED_READ")
        self.assertEqual([item["id"] for item in response["result"]["items"]], ["B", "C"])
        self.assertEqual(response["result"]["classification"], "LOCAL_PRIVATE")


if __name__ == "__main__":
    unittest.main()
