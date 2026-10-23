"""G4 UTF-8 model identifiers: synthetic fixtures with source-safe Unicode escapes."""
import json
import unittest

from scripts.studio_agent_api import validate_command
from scripts.avatar_toolkit_v2 import execute_request

VI_HAIR = "T\u00f3c m\u00e1i"
JP_HAIR = "\u9aea\u306e\u6bdb"
VI_SKIRT = "V\u00e1y tr\u01b0\u1edbc"
JP_EYES = "\u76ee \u3068 \u9f3b"


def wire(op, args):
    return json.dumps({
        "api_version": 2, "request_id": "req-unicode-001",
        "session_id": None, "expected_model_generation": 100,
        "expected_edit_revision": None,
        "operation": op, "arguments": args, "mode": "read"
    }, ensure_ascii=True).encode("utf-8")


class UnicodeFake:
    def __init__(self):
        self.calls = []
        self.param_ids = [VI_HAIR, JP_HAIR, "ParamBlink"]
        self.part_ids = [VI_SKIRT, JP_EYES]

    def call(self, cmd):
        self.calls.append(cmd)
        op = cmd["op"]
        if op == "studio.epoch":
            return {"ok": True, "receipt": "generation=100"}
        if op == "studio.status":
            return {"ok": True, "receipt": "frame=2 playing=0 expression=0 tab=0 xpbd=0 params=3 parts=2 drawables=3"}
        if cmd.get("generation") != 100:
            return {"ok": False, "receipt": "bad-epoch"}
        names = self.param_ids if op.startswith("param.") else self.part_ids
        if op.endswith("find.at"):
            found = [i for i, name in enumerate(names) if name == cmd["id"]]
            if len(found) != 1:
                return {"ok": False, "receipt": "no unique match"}
            return {"ok": True, "receipt": f"index={found[0]}"}
        i = cmd["index"]
        if op.endswith("id.at"):
            return {"ok": True, "receipt": f"index={i} idhex={names[i].encode('utf-8').hex()}"}
        if op == "param.inspect.at":
            return {"ok": True, "receipt": f"index={i} value=0 low=-1 high=1 pin=0"}
        if op == "part.inspect.at":
            return {"ok": True, "receipt": f"index={i} opacity=.5"}
        raise AssertionError("unexpected command")


class UnicodeIDTests(unittest.TestCase):
    def read(self, op, args, bridge=None):
        return json.loads(execute_request(wire(op, args), local_bridge=bridge, allow_local_private=True))

    def test_utf8_encoded_native_wire_is_single_safe_token(self):
        for op, name in [("param.find.at", VI_HAIR), ("part.find.at", JP_EYES)]:
            prefix = "PARAM_FIND_AT" if op.startswith("param.") else "PART_FIND_AT"
            self.assertEqual(validate_command({"op": op, "generation": 100, "id": name}),
                             prefix + " 100 " + name.encode("utf-8").hex())

    def test_page_decodes_utf8_but_keeps_data_local_private(self):
        bridge = UnicodeFake()
        params = self.read("catalog.parameters.page", {"start": 0, "limit": 2}, bridge)
        self.assertEqual(params["state"], "COMPLETED_READ")
        self.assertEqual([i["id"] for i in params["result"]["items"]], [VI_HAIR, JP_HAIR])
        self.assertEqual(params["result"]["classification"], "LOCAL_PRIVATE")
        parts = self.read("catalog.parts.page", {"start": 0, "limit": 2}, bridge)
        self.assertEqual([i["id"] for i in parts["result"]["items"]], [VI_SKIRT, JP_EYES])

    def test_native_id_reverse_lookup(self):
        bridge = UnicodeFake()
        r = self.read("catalog.parameter.lookup", {"id": JP_HAIR}, bridge)
        self.assertEqual(r["state"], "COMPLETED_READ")
        self.assertEqual(r["result"]["index"], 1)
        self.assertEqual(r["result"]["id"], JP_HAIR)

    def test_invalid_hex_id_response_is_rejected_without_payload_echo(self):
        class Broken(UnicodeFake):
            def call(self, cmd):
                if cmd["op"].endswith("id.at"):
                    return {"ok": True, "receipt": "index=0 idhex=c328"}
                return super().call(cmd)
        res = self.read("catalog.parameters.page", {"start": 0, "limit": 1}, Broken())
        self.assertEqual(res["state"], "REJECTED")
        self.assertNotIn("c328", json.dumps(res))

    def test_control_and_oversize_identifiers_rejected_before_native(self):
        for name in ["a\x00b", "a\nb", "a\tb", "x"*97, "\u4e00"*33, "../secret", "\ud800"]:
            bridge = UnicodeFake()
            res = self.read("catalog.parameter.lookup", {"id": name}, bridge)
            self.assertEqual(res["state"], "REJECTED")
            self.assertFalse(bridge.calls)

    def test_utf8_envelope_is_never_available_without_local_binding(self):
        res = json.loads(execute_request(wire("catalog.parameter.lookup", {"id": JP_HAIR})))
        self.assertEqual(res["state"], "REJECTED")


if __name__ == "__main__":
    unittest.main()
