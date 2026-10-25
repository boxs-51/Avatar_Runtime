"""Native Studio Layer Editor cursor and view-only tools, private model E2E."""
import re,shutil
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def call(t,op,**arguments):
 r=t.call(op,**arguments)
 assert r["ok"],"REJECT_"+op
 return r["receipt"]
def fields(receipt):
 return {k:int(v) for k,v in re.findall(r"([a-z]+)=(-?[0-9]+)",receipt)}
def main():
 model=_private_manifest()
 if model is None or not model.is_file() or not EXE.is_file():
  print("LAYER_EDITOR_E2E_BLOCKED_LOCAL_FIXTURE");return 3
 stamp=model.stat().st_mtime_ns
 root=ROOT/"build/studio-user"/("layer-editor-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True,exist_ok=False)
 try:
  with studio(root/"trial",model) as (bridge,epoch,d):
   t=AgentStudioTools(bridge)
   rig=fields(call(t,"rig.selection",generation=epoch))
   assert rig["drawable"]==d
   call(t,"layer.select",generation=epoch,revision=rig["revision"],drawable=d)
   revision=fields(call(t,"layer.inspect",generation=epoch))["revision"]
   view=fields(call(t,"layer.cursor",generation=epoch))
   assert view["drawable"]==d and view["vertex"]==-1
   assert view["triangle"]==-1 and view["wireframe"]==1 and view["isolated"]==0
   assert not t.call("layer.vertex.select",generation=epoch,revision=revision,index=rig["vertices"])["ok"]
   assert not t.call("layer.vertex.select",generation=epoch,revision=revision+1,index=0)["ok"]
   wrong=epoch-1 if epoch>1 else epoch+1
   assert not t.call("layer.vertex.select",generation=wrong,revision=revision,index=0)["ok"]
   assert fields(call(t,"layer.cursor",generation=epoch))==view
   vertex=fields(call(t,"layer.vertex.select",generation=epoch,revision=revision,index=0))
   assert vertex["vertex"]==0 and vertex["triangle"]==-1
   meta=fields(call(t,"layer.inspect",generation=epoch))
   assert meta["triangles"]>=1
   assert not t.call("layer.triangle.select",generation=epoch,revision=revision,index=meta["triangles"])["ok"]
   tri=fields(call(t,"layer.triangle.select",generation=epoch,revision=revision,index=0))
   assert tri["triangle"]==0 and tri["vertex"]==-1
   assert fields(call(t,"layer.overlay",generation=epoch,revision=revision,enabled=False))["wireframe"]==0
   assert fields(call(t,"layer.isolate",generation=epoch,revision=revision,enabled=True))["isolated"]==1
   assert fields(call(t,"layer.cursor",generation=epoch))["isolated"]==1
   assert fields(call(t,"layer.isolate",generation=epoch,revision=revision,enabled=False))["isolated"]==0
   status=call(t,"studio.status")
   total=int(re.search(r"drawables=([0-9]+)",status).group(1))
   switched=False
   for target in range(min(total,128)):
    if target==d:continue
    if not t.call("g4.mesh.meta",generation=epoch,drawable=target)["ok"]:continue
    if not t.call("layer.select",generation=epoch,revision=revision,drawable=target)["ok"]:continue
    switched=True
    fresh=fields(call(t,"layer.cursor",generation=epoch))
    assert fresh["drawable"]==target and fresh["vertex"]==-1
    assert fresh["triangle"]==-1 and fresh["isolated"]==0
    assert not t.call("layer.vertex.select",generation=epoch,revision=revision,index=0)["ok"]
    break
   assert switched,"NO_SECOND_DRAWABLE"
  assert model.stat().st_mtime_ns==stamp,"SOURCE_MODEL_CHANGED"
  print("LAYER_EDITOR_NATIVE_PASS cursor=1 vertex=1 triangle=1 overlay=1 isolate=1 stale=1 swap=1 model_unchanged=1")
  return 0
 finally:shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
