"""Private sidecar restart, wrong signature and corrupt-load atomicity."""
from datetime import datetime
import shutil
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def status(t,e):
 r=t.call("layer.project.status",generation=e)
 assert r["ok"],"STATUS"
 return {k:int(v) for k,v in (piece.split("=",1) for piece in r["receipt"].split())}
def cmd(t,e,op,**data):
 r=t.call(op,generation=e,revision=status(t,e)["revision"],**data)
 assert r["ok"],"REJECT_"+op
def main():
 model=_private_manifest()
 if not model or not model.is_file() or not EXE.is_file():
  print("GEOM_RESTART_BLOCKED");return 3
 root=ROOT/"build/studio-user"/("geom-restart-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True)
 stamp=model.stat().st_mtime_ns
 try:
  a=root/"a";b=root/"b";b.mkdir()
  with studio(a,model) as (bridge,epoch,d):
   t=AgentStudioTools(bridge)
   cmd(t,epoch,"layer.create",id=17)
   for x,y in ((0.,0.),(2.,0.),(0.,2.)):
    cmd(t,epoch,"layer.point.add",layer=0,x=x,y=y)
   cmd(t,epoch,"layer.face.add",layer=0,a=0,b=1,c=2)
   cmd(t,epoch,"layer.project.save")
   assert (a/"layer_geometry.txt").exists(),"NO_SIDECAR"
  shutil.copyfile(a/"layer_geometry.txt",b/"layer_geometry.txt")
  with studio(b,model) as (bridge,fresh,d):
   assert fresh!=epoch,"SAME_EPOCH"
   t=AgentStudioTools(bridge)
   assert status(t,fresh)["layers"]==0,"RESTART_NOT_EMPTY"
   assert t.call("layer.project.load",generation=epoch,revision=0)["ok"] is False
   cmd(t,fresh,"layer.project.load")
   assert status(t,fresh)["layers"]==1,"LOAD_NOT_PERSISTENT"
   old=status(t,fresh)
   with (b/"layer_geometry.txt").open("wb") as f:f.write(b"bad sidecar")
   bad=t.call("layer.project.load",generation=fresh,revision=old["revision"])
   assert not bad["ok"],"CORRUPT_ACCEPTED"
   assert status(t,fresh)==old,"CORRUPT_MUTATED_DOCUMENT"
  assert model.stat().st_mtime_ns==stamp,"SOURCE_EDITED"
  print("GEOM_RESTART_PASS persistent=1 new_epoch=1 stale=1 corrupt_atomic=1 model_private=1")
  return 0
 finally:shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
