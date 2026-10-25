"""Native Studio local geometry editing E2E; private model unchanged."""
import shutil
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def cmd(t,name,**kwargs):
 result=t.call(name,**kwargs)
 assert result["ok"],"REJECT_"+name
 return result["receipt"]
def status(t,epoch):
 raw=cmd(t,"layer.project.status",generation=epoch)
 return {k:int(v) for k,v in
   (token.split("=",1) for token in raw.split(" "))}
def main():
 model=_private_manifest()
 if not model or not model.is_file() or not EXE.is_file():
  print("GEOM_E2E_BLOCKED");return 3
 modified=model.stat().st_mtime_ns
 root=ROOT/"build/studio-user"/("geom-e2e-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True,exist_ok=False)
 try:
  folder=root/"session"
  with studio(folder,model) as (bridge,epoch,d):
   t=AgentStudioTools(bridge)
   before=status(t,epoch);assert before["layers"]==0
   def op(name,**kw):
    v=status(t,epoch)["revision"]
    return cmd(t,name,generation=epoch,revision=v,**kw)
   op("layer.create",id=1)
   op("layer.point.add",layer=0,x=0.,y=0.)
   op("layer.point.add",layer=0,x=2.,y=0.)
   op("layer.point.add",layer=0,x=0.,y=2.)
   op("layer.face.add",layer=0,a=0,b=1,c=2)
   meta=t.call("layer.project.layer",generation=epoch,layer=0)
   assert meta["ok"] and "vertices=3" in meta["receipt"] and "triangles=1" in meta["receipt"]
   point=t.call("layer.project.point",generation=epoch,layer=0,vertex=0)
   assert point["ok"] and "x=0" in point["receipt"]
   face=t.call("layer.project.face",generation=epoch,layer=0,triangle=0)
   assert face["ok"] and "a=0 b=1 c=2" in face["receipt"]
   assert not t.call("layer.project.point",generation=epoch,layer=0,vertex=999)["ok"]
   assert not t.call("layer.point.delete",generation=epoch,revision=status(t,epoch)["revision"],layer=0,vertex=1)["ok"]
   assert not t.call("layer.point.move",generation=epoch,revision=0,layer=0,vertex=0,x=.5,y=.5)["ok"]
   op("layer.point.move",layer=0,vertex=0,x=.5,y=.5)
   op("layer.edit.undo")
   op("layer.edit.redo")
   op("layer.create",id=2)
   assert status(t,epoch)["layers"]==2
   op("layer.delete",layer=1)
   assert status(t,epoch)["layers"]==1
   op("layer.edit.undo")
   assert status(t,epoch)["layers"]==2
   op("layer.project.save")
   assert (folder/"layer_geometry.txt").is_file()
   op("layer.delete",layer=0)
   assert status(t,epoch)["layers"]==1
   op("layer.project.load")
   assert status(t,epoch)["layers"]==2
   op("layer.duplicate",layer=0,id=3)
   assert status(t,epoch)["layers"]==3
   old_revision=status(t,epoch)["revision"]
   op("layer.reorder",layer=2,target=0)
   assert status(t,epoch)["layers"]==3
   assert not t.call("layer.project.by_id",generation=epoch,
       revision=old_revision,id=3)["ok"],"STALE_ID_READ_ACCEPTED"
   def stable(id):
    reply=t.call("layer.project.by_id",generation=epoch,
       revision=status(t,epoch)["revision"],id=id)
    assert reply["ok"],"MISSING_ID"
    return {k:int(v) for k,v in
        (part.split("=",1) for part in reply["receipt"].split())}
   assert stable(3)["layer"]==0 and stable(1)["layer"]==1
   assert stable(2)["layer"]==2
   active=t.call("layer.active.id",generation=epoch,
       revision=status(t,epoch)["revision"],id=2)
   assert active["ok"] and "layer=2" in active["receipt"]
   assert not t.call("layer.active.id",generation=epoch,
       revision=old_revision,id=3)["ok"],"STALE_ID_SELECT_ACCEPTED"
   assert not t.call("layer.active.id",generation=epoch,
       revision=status(t,epoch)["revision"],id=999)["ok"]
   op("layer.delete",layer=0)
   assert status(t,epoch)["layers"]==2
   assert not t.call("layer.project.by_id",generation=epoch,
       revision=old_revision,id=3)["ok"],"ABA_ID_REQUEST_ACCEPTED"
   assert not t.call("layer.project.by_id",generation=epoch,
       revision=status(t,epoch)["revision"],id=3)["ok"],"DELETED_ID_VISIBLE"
   op("layer.create",id=84)
   index=stable(84)["layer"]
   for i in range(4):
    op("layer.point.add",layer=index,x=float(i),y=float(i))
   def point(v):
    out=t.call("layer.project.point",generation=epoch,layer=index,vertex=v)
    assert out["ok"]
    return {k:int(value) for k,value in
       (segment.split("=",1) for segment in out["receipt"].split())
       if k in ("vertex_id","layer","index","revision")}
   third=point(2)["vertex_id"]
   fourth=point(3)["vertex_id"]
   op("layer.point.delete",layer=index,vertex=1)
   assert point(1)["vertex_id"]==third
   assert point(2)["vertex_id"]==fourth
   op("layer.point.add",layer=index,x=5.,y=5.)
   assert point(3)["vertex_id"] not in (third,fourth)
   def lookup(uid,revision=None):
    if revision is None:revision=status(t,epoch)["revision"]
    return t.call("layer.project.vertex_by_id",generation=epoch,
       revision=revision,id=84,vertex_id=uid)
   resolved=lookup(third)
   assert resolved["ok"] and "vertex=1" in resolved["receipt"]
   old=status(t,epoch)["revision"]
   op("layer.point.move",layer=index,vertex=1,x=9.,y=9.)
   assert not lookup(third,old)["ok"],"STALE_VERTEX_ID_ACCEPTED"
   op("layer.project.save")
   op("layer.model.compile")
   op("layer.model.open")
   assert lookup(third)["ok"] and "vertex=1" in lookup(third)["receipt"]
   assert not t.call("layer.project.status",generation=epoch-1 if epoch>1 else epoch+1)["ok"]
  assert model.stat().st_mtime_ns==modified
  print("GEOM_NATIVE_PASS create=1 vertices=1 faces=1 move=1 delete=1 undo_redo=1 save_load=1 source_unchanged=1")
  return 0
 finally:shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
