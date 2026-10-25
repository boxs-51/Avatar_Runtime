"""Import a bounded MOC3 drawable as editable local sidecar geometry."""
import re,shutil
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def values(receipt):
 return {k:int(v) for k,v in re.findall(r"([a-z]+)=(-?[0-9]+)",receipt)}
def main():
 model=_private_manifest()
 if model is None or not model.is_file() or not EXE.is_file():
  print("GEOM_IMPORT_BLOCKED_FIXTURE");return 3
 stamp=model.stat().st_mtime_ns
 root=ROOT/"build/studio-user"/("geometry-import-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True,exist_ok=False)
 try:
  with studio(root/"session",model) as (bridge,epoch,_):
   tools=AgentStudioTools(bridge)
   r=tools.call("studio.status")
   assert r["ok"],"STUDIO_STATUS"
   total=int(values(r["receipt"])["drawables"])
   hit=False
   for drawable in range(min(total,128)):
    r=tools.call("g4.mesh.meta",generation=epoch,drawable=drawable)
    if not r["ok"]:continue
    data=values(r["receipt"])
    if data["vertices"]>128 or data["triangles"]>256:continue
    command=tools.call("layer.copy.drawable",generation=epoch,revision=0,
                       drawable=drawable,id=60)
    if not command["ok"]:continue
    hit=True;break
   assert hit,"NO_BOUNDED_DRAWABLE_IMPORT"
   r=tools.call("layer.project.status",generation=epoch)
   assert r["ok"]
   current=values(r["receipt"])
   assert current["layers"]==1 and current["revision"]==1
   out=tools.call("layer.project.save",generation=epoch,revision=1)
   assert out["ok"]
   assert (root/"session/layer_geometry.txt").is_file()
  assert model.stat().st_mtime_ns==stamp,"SOURCE_MODEL_CHANGED"
  print("GEOM_IMPORT_NATIVE_PASS copy_compiled_to_sidecar=1 revision=1 private_save=1 source_unchanged=1")
  return 0
 finally:shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
