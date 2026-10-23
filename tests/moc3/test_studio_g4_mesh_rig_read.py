"""G4 mesh, topology and rig local read, synthetic fixture."""
import json, unittest
from scripts.avatar_toolkit_v2 import execute_request
from scripts.studio_agent_api import validate_command
def ask(op,args,bridge,epoch=55,allow=True):
 data={"api_version":2,"request_id":"req-g4-read","session_id":None,
       "expected_model_generation":epoch,"expected_edit_revision":None,
       "operation":op,"arguments":args,"mode":"read"}
 return json.loads(execute_request(json.dumps(data).encode(),local_bridge=bridge,allow_local_private=allow))
class NativeFake:
 def __init__(self,drift=False,bad=False):
  self.calls=[];self.drift=drift;self.bad=bad;self.epochs=0
 def call(self,c):
  self.calls.append(c);op=c["op"]
  if op=="studio.epoch":
   self.epochs+=1
   return {"ok":True,"receipt":"generation="+str(56 if self.drift and self.epochs>2 else 55)}
  if op=="studio.status":
   return {"ok":True,"receipt":"frame=1 playing=0 expression=0 tab=3 xpbd=0 params=2 parts=1 drawables=2"}
  if op=="g4.mesh.vertex_count" and c.get("generation")==55 and c.get("drawable")==1:
   return {"ok":True,"receipt":"drawable=1 vertices=3"}
  if c.get("generation")!=55 or c.get("drawable")!=0:
   return {"ok":False,"receipt":"rejected"}
  if op=="g4.mesh.meta":reply="drawable=0 vertices=3 triangles=1 islands=1 used=3 loose=0 largest=3"
  elif op=="g4.mesh.vertex":reply=f"drawable=0 index={c['index']} x={c['index']}.5 y=0"
  elif op=="g4.mesh.triangle":reply="drawable=0 index=0 a=0 b=1 c=2"
  elif op=="g4.rig.revision":reply="revision=1"
  elif op=="g4.rig.meta":reply="drawable=0 vertices=3 role=0 anchors=1 beams=1 parents=1 weights=1 cross=1"
  elif op=="g4.rig.entry":
   reply={"anchor":"kind=anchor index=0 vertex=0","beam":"kind=beam index=0 a=0 b=1",
          "parent":"kind=parent index=0 parent=0 child=1 stiffness=.8",
          "weight":"kind=weight index=0 vertex=2 weight=.5",
          "cross":"kind=cross index=0 parent=0 target=1 child=2 stiffness=.9"}[c["kind"]]
  else:raise AssertionError(op)
  if self.bad and op=="g4.mesh.vertex":reply="drawable=0 index=0 x=nan y=0"
  return {"ok":True,"receipt":reply}
class G4Tests(unittest.TestCase):
 def test_wire(self):
  cases=[
   ({"op":"g4.mesh.meta","generation":55,"drawable":0},"G4_MESH_META 55 0"),
   ({"op":"g4.mesh.vertex","generation":55,"drawable":0,"index":1},"G4_MESH_VERTEX 55 0 1"),
   ({"op":"g4.mesh.triangle","generation":55,"drawable":0,"index":0},"G4_MESH_TRI 55 0 0"),
   ({"op":"g4.rig.meta","generation":55,"drawable":0},"G4_RIG_META 55 0"),
   ({"op":"g4.rig.entry","generation":55,"drawable":0,"kind":"weight","index":0},"G4_RIG_ENTRY 55 0 weight 0")]
  for cmd,want in cases:
   with self.subTest(op=cmd["op"]):self.assertEqual(validate_command(cmd),want)
 def test_topology_and_vertex_triangle_pages(self):
  f=NativeFake()
  t=ask("mesh.topology.inspect",{"drawable":0},f)
  self.assertEqual(t["state"],"COMPLETED_READ")
  self.assertEqual(t["result"]["topology"]["islands"],1)
  v=ask("mesh.vertices.page",{"drawable":0,"start":1,"limit":2},f)
  self.assertEqual(v["state"],"COMPLETED_READ")
  self.assertEqual([x["index"] for x in v["result"]["items"]],[1,2])
  tri=ask("mesh.triangles.page",{"drawable":0,"start":0,"limit":1},f)
  self.assertEqual(tri["result"]["items"][0]["vertices"],[0,1,2])
 def test_rig_meta_and_entries(self):
  f=NativeFake()
  m=ask("rig.local.inspect",{"drawable":0},f)
  self.assertEqual(m["state"],"COMPLETED_READ")
  self.assertTrue(m["result"]["editor_revision_available"])
  self.assertEqual(m["result"]["editor_revision"],1)
  for kind in ("anchor","beam","parent","weight","cross"):
   with self.subTest(kind=kind):
    p=ask("rig.entries.page",{"drawable":0,"kind":kind,"start":0,"limit":1},f)
    self.assertEqual(p["state"],"COMPLETED_READ")
    self.assertEqual(p["result"]["items"][0]["kind"],kind)
 def test_invalid_and_untrusted(self):
  for op,args in (("mesh.vertices.page",{"drawable":0,"start":0,"limit":5}),
                  ("mesh.topology.inspect",{"drawable":True}),
                  ("rig.entries.page",{"drawable":0,"kind":"bad","start":0,"limit":1}),
                  ("rig.local.inspect",{"drawable":0,"path":"forbidden"})):
   f=NativeFake();r=ask(op,args,f)
   self.assertEqual(r["state"],"REJECTED")
   self.assertEqual(f.calls,[])
  f=NativeFake()
  self.assertEqual(ask("mesh.topology.inspect",{"drawable":0},f,allow=False)["error"]["code"],"PERMISSION_DENIED")
  self.assertEqual(f.calls,[])
 def test_generation_and_malformed(self):
  r=ask("mesh.vertices.page",{"drawable":0,"start":0,"limit":2},NativeFake(drift=True))
  self.assertEqual(r["error"]["code"],"STALE_MODEL")
  r=ask("mesh.vertices.page",{"drawable":0,"start":0,"limit":1},NativeFake(bad=True))
  self.assertEqual(r["state"],"REJECTED")
  self.assertIsNone(r["result"])
if __name__=="__main__":unittest.main()
