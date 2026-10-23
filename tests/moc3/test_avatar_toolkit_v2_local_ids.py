"""RED-first model-ID page and exact lookup contract (synthetic)."""
import json,unittest
from scripts.avatar_toolkit_v2 import execute_request

def ask(op,args=None,bridge=None,epoch=100):
 wire=json.dumps({"api_version":2,"request_id":"req-id-test-01",
 "session_id":None,"expected_model_generation":epoch,
 "expected_edit_revision":None,"operation":op,"arguments":args or {},"mode":"read"}).encode()
 return json.loads(execute_request(wire,local_bridge=bridge,allow_local_private=True))

class Fake:
 def __init__(self):self.calls=[];self.p=["ParamAngleX","ParamAngleY"];self.r=["PartHair","PartBody"]
 def call(self,c):
  self.calls.append(c);op=c["op"]
  if op=="studio.epoch":return {"ok":True,"receipt":"generation=100"}
  if op=="studio.status":return {"ok":True,"receipt":"frame=3 playing=0 expression=0 tab=2 xpbd=0 params=2 parts=2 drawables=3"}
  if c["generation"]!=100:return {"ok":False,"receipt":"rejected"}
  names=self.p if op.startswith("param.") else self.r
  if op.endswith("find.at"):
   found=[i for i,n in enumerate(names) if n==c["id"]]
   return {"ok":len(found)==1,"receipt":f"index={found[0]}" if len(found)==1 else "rejected"}
  i=c["index"]
  if op.endswith("id.at"):return {"ok":True,"receipt":f"index={i} id={names[i]}"}
  if op=="param.inspect.at":return {"ok":True,"receipt":f"index={i} value=0 low=-1 high=1 pin=0"}
  return {"ok":True,"receipt":f"index={i} opacity=.5"}

class IDTests(unittest.TestCase):
 def test_id_pages(self):
  for op,names in (("catalog.parameters.page",["ParamAngleX","ParamAngleY"]),
                   ("catalog.parts.page",["PartHair","PartBody"])):
   r=ask(op,{"start":0,"limit":2},Fake())
   self.assertEqual(r["state"],"COMPLETED_READ")
   self.assertEqual([x["id"] for x in r["result"]["items"]],names)
 def test_exact_find(self):
  for op,name,index in (("catalog.parameter.lookup","ParamAngleY",1),
                        ("catalog.part.lookup","PartHair",0)):
   r=ask(op,{"id":name},Fake())
   self.assertEqual(r["result"]["index"],index)
 def test_reject_ambiguous_unknown_and_unsafe(self):
  f=Fake();f.p[1]="ParamAngleX"
  self.assertEqual(ask("catalog.parameter.lookup",{"id":"ParamAngleX"},f)["state"],"REJECTED")
  for bad in ("../private","Name With Spaces","é","", "X"*70,True):
   f=Fake();r=ask("catalog.parameter.lookup",{"id":bad},f)
   self.assertEqual(r["state"],"REJECTED")
   self.assertFalse(f.calls)
 def test_stale_generation_denied(self):
  r=ask("catalog.part.lookup",{"id":"PartHair"},Fake(),101)
  self.assertEqual(r["state"],"REJECTED")
  self.assertEqual(r["error"]["code"],"STALE_MODEL")
