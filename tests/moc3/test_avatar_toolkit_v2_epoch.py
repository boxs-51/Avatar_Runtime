"""RED first: model epoch must reject same-count model replacement."""
import json,unittest
from scripts.avatar_toolkit_v2 import execute_request
def req(op,args=None):
 return json.dumps({"api_version":2,"request_id":"req-epoch-test",
  "session_id":None,"expected_model_generation":None,
  "expected_edit_revision":None,"operation":op,
  "arguments":args or {},"mode":"read"}).encode()

class Fake:
 def __init__(self,drift=False):
  self.calls=[];self.drft=drift;self.epochs=0
 def call(self,command):
  self.calls.append(command)
  op=command["op"]
  if op=="studio.epoch":
   self.epochs+=1
   return {"ok":True,"receipt":"generation="+str(200 if self.drft and self.epochs>1 else 100)}
  if op=="studio.status":
   return {"ok":True,"receipt":"frame=20 playing=1 expression=0 tab=2 xpbd=1 params=5 parts=3 drawables=4"}
  if op=="param.id.at":
   return {"ok":True,"receipt":f"index={command['index']} id=ParamSynthetic{command['index']}"}
  if op=="part.id.at":
   return {"ok":True,"receipt":f"index={command['index']} id=PartSynthetic{command['index']}"}
  if op=="param.inspect.at":
   if command.get("generation")!=100:raise AssertionError("unfenced request")
   return {"ok":True,"receipt":"index=1 value=.5 low=0 high=1 pin=0"}
  if op=="part.inspect.at":
   if command.get("generation")!=100:raise AssertionError("unfenced request")
   return {"ok":True,"receipt":"index=1 opacity=.7"}
  raise AssertionError("unfenced legacy command "+op)

class EpochReadTests(unittest.TestCase):
 def call(self,operation,args=None,bridge=None):
  return json.loads(execute_request(req(operation,args),
    local_bridge=bridge,allow_local_private=True))
 def test_epoch_reported_in_status(self):
  result=self.call("studio.local.status",bridge=Fake())
  self.assertEqual(result["state"],"COMPLETED_READ")
  self.assertEqual(result["result"]["model_generation"],100)
  self.assertTrue(result["result"]["generation_available"])
 def test_paging_guarded_by_native_generation(self):
  bridge=Fake()
  response=self.call("catalog.parameters.page",{"start":1,"limit":1},bridge)
  self.assertEqual(response["state"],"COMPLETED_READ")
  self.assertEqual(response["result"]["model_generation"],100)
  self.assertTrue(response["result"]["generation_available"])
  self.assertIn({"op":"param.inspect.at","generation":100,"index":1},bridge.calls)
 def test_same_count_reload_rejects(self):
  response=self.call("catalog.parameters.page",{"start":1,"limit":1},Fake(drift=True))
  self.assertEqual(response["state"],"REJECTED")
  self.assertEqual(response["error"]["code"],"STALE_MODEL")
 def test_expected_generation_cas(self):
  for generation,want in ((100,"COMPLETED_READ"),(101,"REJECTED")):
   envelope=json.loads(req("catalog.parameters.page",{"start":1,"limit":1}))
   envelope["expected_model_generation"]=generation
   response=json.loads(execute_request(json.dumps(envelope).encode("utf-8"),
       local_bridge=Fake(),allow_local_private=True))
   self.assertEqual(response["state"],want)
   if want=="REJECTED":self.assertEqual(response["error"]["code"],"STALE_MODEL")
 def test_expected_generation_requires_local_binding(self):
  envelope=json.loads(req("studio.local.status"))
  envelope["expected_model_generation"]=100
  response=json.loads(execute_request(json.dumps(envelope).encode("utf-8")))
  self.assertEqual(response["state"],"REJECTED")
 def test_invalid_generation_boolean_fails(self):
  envelope=json.loads(req("studio.local.status"))
  envelope["expected_model_generation"]=True
  response=json.loads(execute_request(json.dumps(envelope).encode("utf-8"),
     local_bridge=Fake(),allow_local_private=True))
  self.assertEqual(response["state"],"REJECTED")
 def test_native_epoch_missing_does_not_fallback_unfenced(self):
  class NoEpoch(Fake):
   def call(self,command):
    if command["op"]=="studio.epoch":return {"ok":False,"receipt":"rejected"}
    return super().call(command)
  response=self.call("catalog.parts.page",{"start":1,"limit":1},NoEpoch())
  self.assertEqual(response["state"],"REJECTED")
