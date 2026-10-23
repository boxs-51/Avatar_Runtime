"""ADR-002 local readback tests using fake v1 bridge, never real model."""
import json,unittest
from scripts.avatar_toolkit_v2 import execute_request
def req(op,args=None):
 return json.dumps({"api_version":2,"request_id":"req-read-1",
 "session_id":None,"expected_model_generation":None,
 "expected_edit_revision":None,"operation":op,"arguments":args or {},
 "mode":"read"}).encode()
class FakeBridge:
 def __init__(self):self.calls=[]
 def call(self,command):
  self.calls.append(command)
  op=command["op"]
  if op=="studio.epoch":
   return {"ok":True,"receipt":"generation=100"}
  if op=="studio.status":
   return {"ok":True,"receipt":"frame=8 playing=1 expression=0 tab=2 xpbd=1 params=5 parts=3 drawables=4"}
  if op=="param.id.at":
   return {"ok":True,"receipt":f"index={command['index']} id=ParamSynthetic{command['index']}"}
  if op=="part.id.at":
   return {"ok":True,"receipt":f"index={command['index']} id=PartSynthetic{command['index']}"}
  if op=="param.inspect.at":
   i=command["index"]
   return {"ok":True,"receipt":f"index={i} value=.5 low=0 high=1 pin=0"}
  if op=="part.inspect.at":
   i=command["index"]
   return {"ok":True,"receipt":f"index={i} opacity=.75"}
  return {"ok":False,"receipt":"rejected"}
class LocalReadTests(unittest.TestCase):
 def test_disconnected(self):
  for op in ("studio.local.status","catalog.parameters.page","catalog.parts.page"):
   self.assertEqual(json.loads(execute_request(req(op)))["state"],"REJECTED")
 def test_permission_required(self):
  b=FakeBridge()
  self.assertEqual(json.loads(execute_request(req("studio.local.status"),local_bridge=b))["state"],"REJECTED")
  self.assertEqual(b.calls,[])
 def test_bound_capabilities_truthful(self):
  b=FakeBridge()
  a=json.loads(execute_request(req("avatar.capabilities"),
       local_bridge=b,allow_local_private=True))
  self.assertEqual(a["state"],"COMPLETED_READ")
  self.assertIn("catalog.parameters.page",a["result"]["supported_operations"])
  self.assertTrue(a["result"]["local_private_read_enabled"])
  self.assertFalse(a["result"]["native_v2_enabled"])
  schema=json.loads(execute_request(req("avatar.schema",
      {"operation":"catalog.parameters.page"}),local_bridge=b,
      allow_local_private=True))
  self.assertEqual(schema["result"]["result_classification"],"LOCAL_PRIVATE")
  self.assertEqual(b.calls,[])
 def test_status(self):
  b=FakeBridge()
  out=json.loads(execute_request(req("studio.local.status"),local_bridge=b,allow_local_private=True))
  self.assertEqual(out["result"]["frame"],8)
  self.assertEqual(out["result"]["counts"],{"params":5,"parts":3,"drawables":4})
  self.assertEqual(out["result"]["classification"],"LOCAL_PRIVATE")
 def test_parameter_paging(self):
  b=FakeBridge()
  out=json.loads(execute_request(req("catalog.parameters.page",{"start":1,"limit":2}),local_bridge=b,allow_local_private=True))
  self.assertEqual([i["index"] for i in out["result"]["items"]],[1,2])
  self.assertEqual(len(b.calls),10)
 def test_page_model_count_changes_fail_closed(self):
  class Changed(FakeBridge):
   def __init__(self):
    super().__init__()
    self.status_queries=0
   def call(self,command):
    response=super().call(command)
    if command=={"op":"studio.status"}:
     self.status_queries+=1
     if self.status_queries>1:
      response["receipt"]=response["receipt"].replace("params=5","params=6")
    return response
  result=json.loads(execute_request(req("catalog.parameters.page",
        {"start":1,"limit":2}),local_bridge=Changed(),allow_local_private=True))
  self.assertEqual(result["state"],"REJECTED")
  self.assertIsNone(result["result"])
 def test_part_paging(self):
  b=FakeBridge()
  out=json.loads(execute_request(req("catalog.parts.page",{"start":0,"limit":1}),local_bridge=b,allow_local_private=True))
  self.assertEqual(out["result"]["items"][0]["opacity"],.75)
 def test_bad_paging_rejected_before_bridge(self):
  for args in ({"start":-1,"limit":1},{"start":1,"limit":0},
               {"start":1,"limit":10},{"start":True,"limit":1},
               {"start":0,"limit":1,"extra":1}):
   b=FakeBridge()
   with self.subTest(args=args):
    out=json.loads(execute_request(req("catalog.parameters.page",args),
        local_bridge=b,allow_local_private=True))
    self.assertEqual(out["state"],"REJECTED")
    self.assertEqual(b.calls,[])
 def test_malformed_receipt_fail_closed(self):
  class Broken(FakeBridge):
   def call(self,command):
    return {"ok":True,"receipt":"frame=8 playing=1 private-path"}
  out=json.loads(execute_request(req("studio.local.status"),
      local_bridge=Broken(),allow_local_private=True))
  self.assertEqual(out["state"],"REJECTED")
  self.assertNotIn("private-path",json.dumps(out))
 def test_timeout_unknown(self):
  class Timed:
   def call(self,cmd):raise TimeoutError("private string")
  out=json.loads(execute_request(req("studio.local.status"),
      local_bridge=Timed(),allow_local_private=True))
  self.assertEqual(out["state"],"REJECTED")
  self.assertEqual(out["error"]["code"],"TIMEOUT_UNKNOWN")
  self.assertNotIn("private string",json.dumps(out))
