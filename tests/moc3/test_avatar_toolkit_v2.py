"""RED-first tests for the isolated ADR-002 v2 discovery route."""
import json,unittest
from scripts.avatar_toolkit_v2 import execute_request,decode_request,ToolkitV2Error
def query(op="avatar.capabilities",arguments=None,**update):
    msg={"api_version":2,"request_id":"req-test-0001","session_id":None,
         "expected_model_generation":None,"expected_edit_revision":None,
         "operation":op,"arguments":arguments or {},"mode":"read"}
    msg.update(update)
    return json.dumps(msg).encode("utf-8")
class V2DiscoveryTests(unittest.TestCase):
    def test_readonly_catalog(self):
        out=json.loads(execute_request(query()))
        self.assertEqual(out["state"],"COMPLETED_READ")
        caps=out["result"]
        self.assertEqual(caps["supported_operations"],["avatar.capabilities","avatar.schema"])
        self.assertFalse(caps["native_v2_enabled"])
        self.assertFalse(caps["mutating_commands_enabled"])
        self.assertEqual(caps["release_gate"],"HOLD")
    def test_schema(self):
        out=json.loads(execute_request(query("avatar.schema",{"operation":"avatar.capabilities"})))
        self.assertEqual(out["result"]["operation"],"avatar.capabilities")
        self.assertEqual(out["result"]["effect"],"read")
    def test_operations_fail_closed(self):
        for op in ("rig.constraint.upsert","draft.commit","session.open",
                   "catalog.drawables","viewport.capture","exec"):
            with self.subTest(op=op):
                reply=json.loads(execute_request(query(op)))
                self.assertEqual(reply["state"],"REJECTED")
    def test_generation_and_unknown_fields_denied(self):
        for update in ({"session_id":"active"},{"expected_model_generation":7},
                       {"expected_edit_revision":1},{"mode":"stage"},
                       {"api_version":True},{"unexpected":1}):
            with self.subTest(fields=update):
                self.assertEqual(json.loads(execute_request(query(**update)))["state"],"REJECTED")
    def test_wire_edge_cases(self):
        valid=query()
        bad=[
           valid+b" extra",
           valid.replace(b'"mode": "read"',b'"mode": "read", "mode": "read"'),
           valid.replace(b'"api_version": 2',b'"api_version": NaN'),
           valid.replace(b'"api_version": 2',b'"api_version": 1e999'),
           valid.replace(b'"arguments": {}',b'"arguments": {"x": 1e999}'),
           valid.replace(b'"api_version": 2',b'"api_version": 99999999999999999999'),
           b"\xff"+valid,
           valid+b" "*65536,
           b"{",
           b"[]",
        ]
        for message in bad:
            with self.subTest(size=len(message)),self.assertRaises(ToolkitV2Error):
                decode_request(message)
    def test_schema_rejects_private_payload(self):
        for args in ({"operation":"PRIVATE_MODEL_INFO"},
                     {"operation":"avatar.capabilities","path":"hidden"}):
            result=json.loads(execute_request(query("avatar.schema",args)))
            self.assertEqual(result["state"],"REJECTED")
            self.assertNotIn("PRIVATE_MODEL_INFO",json.dumps(result))
    def test_oversized_request_is_bounded(self):
        response=json.loads(execute_request(query("rig.weight.paint",{"raw":"X"*68000})))
        self.assertEqual(response["state"],"REJECTED")
        self.assertLess(len(json.dumps(response)),2048)


    def test_deterministic_decoder_fuzz_no_crash(self):
        from random import Random
        rng=Random(20261023)
        baseline=query()
        for trial in range(256):
            payload=bytearray(baseline)
            if trial%4==0:
                payload.insert(rng.randrange(len(payload)+1),rng.randrange(256))
            elif trial%4==1:
                payload[rng.randrange(len(payload))]=rng.randrange(256)
            elif trial%4==2:
                payload=payload[:rng.randrange(len(payload))]
            else:
                payload.extend(bytes([rng.randrange(256)])*rng.randrange(1,18))
            reply=execute_request(bytes(payload))
            self.assertLess(len(reply),2048)
            result=json.loads(reply)
            self.assertIn(result["state"],("REJECTED","COMPLETED_READ"))
            if result["state"]=="REJECTED":
                self.assertEqual(set(result["error"]),{"code"})
