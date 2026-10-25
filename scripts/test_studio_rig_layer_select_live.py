"""Private Native Studio E2E: RigBank selection and Layer readback."""
import re,shutil
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def parse(t,op,**kw):
 reply=t.call(op,**kw)
 assert reply["ok"],"NATIVE_DENIED_"+op
 return dict((k,int(v)) for k,v in re.findall(r"([a-z]+)=([0-9]+)",reply["receipt"]))
def main():
 model=_private_manifest()
 if model is None or not model.is_file() or not EXE.is_file():
  print("RIG_LAYER_SELECTION_BLOCKED_LOCAL_MODEL");return 3
 root=ROOT/"build/studio-user"/("rig-layer-select-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True,exist_ok=False)
 try:
  with studio(root/"trial",model) as (bridge,epoch,d):
   t=AgentStudioTools(bridge)
   first=parse(t,"rig.selection",generation=epoch)
   assert first["drawable"]==d and first["vertices"]>=1
   layer=parse(t,"layer.inspect",generation=epoch)
   assert layer["drawable"]==d and layer["revision"]==first["revision"]
   assert layer["vertices"]==first["vertices"]
   bad_gen=epoch-1 if epoch>1 else epoch+1
   assert not t.call("rig.select.at",generation=bad_gen,revision=first["revision"],drawable=d)["ok"]
   assert not t.call("rig.select.at",generation=epoch,revision=first["revision"]+1,drawable=d)["ok"]
   status=t.call("studio.status")
   assert status["ok"]
   total=int(re.search(r"drawables=([0-9]+)",status["receipt"]).group(1))
   if total<=2047:
    assert not t.call("layer.select",generation=epoch,revision=first["revision"],drawable=total)["ok"]
   assert parse(t,"rig.selection",generation=epoch)==first
   assert t.call("rig.select.at",generation=epoch,revision=first["revision"],drawable=d)["ok"]
   assert parse(t,"rig.selection",generation=epoch)==first
   t.call("rig.anchor",vertex=0,enabled=True)
   saved=parse(t,"rig.selection",generation=epoch)
   assert saved["revision"]==first["revision"]+1
   meta=parse(t,"layer.inspect",generation=epoch)
   assert meta["revision"]==saved["revision"]
   switched=False
   for target in range(min(total,128)):
    if target==d:continue
    info=t.call("g4.mesh.meta",generation=epoch,drawable=target)
    if not info["ok"]:continue
    outcome=t.call("layer.select",generation=epoch,revision=saved["revision"],drawable=target)
    if not outcome["ok"]:continue
    switched=True
    assert "tab=1" in t.call("studio.status")["receipt"],"LAYER_INSPECTOR_NOT_VISIBLE"
    other=parse(t,"rig.selection",generation=epoch)
    assert other["drawable"]==target and other["revision"]==saved["revision"]+1
    assert parse(t,"layer.inspect",generation=epoch)["drawable"]==target
    assert not t.call("rig.select.at",generation=epoch,revision=saved["revision"],drawable=d)["ok"]
    back=t.call("rig.select.at",generation=epoch,revision=other["revision"],drawable=d)
    assert back["ok"]
    assert "tab=3" in t.call("studio.status")["receipt"],"RIG_INSPECTOR_NOT_VISIBLE"
    assert "anchors=1" in t.call("rig.inspect")["receipt"],"RIGBANK_LOST_EDIT"
    assert parse(t,"rig.selection",generation=epoch)["drawable"]==d
    # Entering Layers from XPBD must synchronize the same selected drawable.
    assert t.call("studio.tab",index=2)["ok"]
    assert t.call("xpbd.select",first=target,second=-1,joint=False)["ok"]
    assert t.call("studio.tab",index=1)["ok"]
    synced=parse(t,"rig.selection",generation=epoch)
    assert synced["drawable"]==target,"LAYER_TAB_RIG_SELECTION_DIVERGED"
    assert parse(t,"layer.inspect",generation=epoch)["drawable"]==target
    break
   assert switched,"SECOND_SELECTABLE_DRAWABLE_NOT_FOUND"
  print("RIG_LAYER_SELECTION_PASS epoch_cas=1 same_selector=1 rigbank=1 layer_read=1 model_private=1")
  return 0
 finally:shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
