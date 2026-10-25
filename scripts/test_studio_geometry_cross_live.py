"""Local Native Studio two-layer identity and Rig link E2E (private assets)."""
import shutil,time
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def kv(result):
 assert result["ok"],"NATIVE_CALL_REJECTED"
 return dict(token.split("=",1) for token in result["receipt"].split() if "=" in token)
def main():
 model=_private_manifest()
 if not model or not model.is_file() or not EXE.is_file():
  print("CROSS_RIG_E2E_BLOCKED_FIXTURE");return 3
 stamp=model.stat().st_mtime_ns
 root=ROOT/"build/studio-user"/("cross-rig-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True,exist_ok=False)
 try:
  with studio(root/"session",model) as (bridge,epoch,_):
   t=AgentStudioTools(bridge)
   def current():
    return int(kv(t.call("layer.project.status",generation=epoch))["revision"])
   def edit(name,**kw):
    return kv(t.call(name,generation=epoch,revision=current(),**kw))
   for layer_id,dx in ((101,0.),(102,2.)):
    edit("layer.create",id=layer_id)
    pos=int(kv(t.call("layer.project.by_id",generation=epoch,
                         revision=current(),id=layer_id))["layer"])
    for x,y in ((0.,0.),(1.,0.),(0.,2.)):
     edit("layer.point.add",layer=pos,x=x+dx,y=y)
    edit("layer.face.add",layer=pos,a=0,b=1,c=2)
    edit("layer.rig.pin",layer=pos,vertex=0)
   def ids(layer_id,vertex):
    pos=int(kv(t.call("layer.project.by_id",generation=epoch,
       revision=current(),id=layer_id))["layer"])
    return int(kv(t.call("layer.project.point",generation=epoch,
                           layer=pos,vertex=vertex))["vertex_id"])
   a=ids(101,2);b=ids(102,1)
   before=current()
   edit("layer.rig.link",parent_id=101,parent_vertex_id=a,
                      child_id=102,child_vertex_id=b,stiffness=.8)
   assert int(kv(t.call("layer.rig.links",generation=epoch))["links"])==1
   link=kv(t.call("layer.rig.link.inspect",generation=epoch,
        revision=current(),index=0))
   assert link["parent_id"]=="101" and link["child_id"]=="102"
   assert int(link["parent_vertex_id"])==a and int(link["child_vertex_id"])==b
   assert not t.call("layer.rig.link",generation=epoch,revision=before,
          parent_id=101,parent_vertex_id=a,child_id=102,
          child_vertex_id=b,stiffness=.8)["ok"],"STALE_RIG_LINK_ACCEPTED"

   assert not t.call("layer.rig.link",generation=epoch,revision=current(),
          parent_id=101,parent_vertex_id=a,child_id=102,
          child_vertex_id=b,stiffness=.8)["ok"],"DUPLICATE_LINK_ACCEPTED"
   assert not t.call("layer.rig.link",generation=epoch,revision=current(),
          parent_id=102,parent_vertex_id=b,child_id=101,
          child_vertex_id=a,stiffness=.8)["ok"],"CYCLIC_LINK_ACCEPTED"
   assert not t.call("layer.point.delete",generation=epoch,revision=current(),
          layer=1,vertex=1)["ok"],"LINKED_VERTEX_DELETED"
   assert not t.call("layer.delete",generation=epoch,revision=current(),
          layer=0)["ok"],"LINKED_LAYER_DELETED"
   edit("layer.reorder",layer=0,target=1)
   assert int(kv(t.call("layer.project.by_id",generation=epoch,
        revision=current(),id=101))["layer"])==1
   assert ids(101,2)==a and ids(102,1)==b
   edit("layer.project.save")
   edit("layer.model.compile")
   edit("layer.model.open")
   assert int(kv(t.call("layer.rig.links",generation=epoch))["links"])==1
   edit("layer.project.load")
   assert int(kv(t.call("layer.rig.links",generation=epoch))["links"])==1
   assert ids(101,2)==a and ids(102,1)==b
   second_index=int(kv(t.call("layer.project.by_id",generation=epoch,
      revision=current(),id=102))["layer"])
   edit("layer.physics.bind",layer=second_index)
   physical={}
   for attempt in range(75):
    physical=kv(t.call("layer.physics.status",generation=epoch))
    if int(physical["accepted"])>=5 and physical["gpu"]=="1":break
    time.sleep(.06)
   assert int(physical.get("accepted","0"))>=5,"NO_TWO_LAYER_GPU_ACCEPTANCE"
   p101=kv(t.call("layer.physics.point_by_id",generation=epoch,
                 id=101,vertex_id=a))
   p102=kv(t.call("layer.physics.point_by_id",generation=epoch,
                 id=102,vertex_id=b))
   assert p101["gpu"]=="1" and p102["gpu"]=="1"
   assert int(p101["accepted"])>0 and int(p102["accepted"])>0
   assert all(abs(float(part[k]))<10000 for part in (p101,p102) for k in ("x","y"))
   assert abs(float(p101["y"])-2.0)>0.00001,"LAYER_PARENT_NOT_MOVING"
   assert abs(float(p102["y"]))>0.00001,"LAYER_CHILD_NOT_MOVING"
   anchor101=kv(t.call("layer.physics.point_by_id",generation=epoch,
                         id=101,vertex_id=ids(101,0)))
   anchor102=kv(t.call("layer.physics.point_by_id",generation=epoch,
                         id=102,vertex_id=ids(102,0)))
   assert abs(float(anchor101["x"]))<.05 and abs(float(anchor101["y"]))<.05
   assert abs(float(anchor102["x"])-2.)<.05 and abs(float(anchor102["y"]))<.05
   edit("layer.physics.stop")
   assert not t.call("layer.physics.point_by_id",generation=epoch,
                      id=102,vertex_id=b)["ok"],"STALE_PHYSICAL_READBACK"
   edit("layer.create",id=103)
   third_index=int(kv(t.call("layer.project.by_id",generation=epoch,
                 revision=current(),id=103))["layer"])
   for x,y in ((4.,0.),(5.,0.),(4.,2.)):
    edit("layer.point.add",layer=third_index,x=x,y=y)
   edit("layer.face.add",layer=third_index,a=0,b=1,c=2)
   edit("layer.rig.pin",layer=third_index,vertex=0)
   c=ids(103,2)
   edit("layer.rig.link",parent_id=102,parent_vertex_id=b,
             child_id=103,child_vertex_id=c,stiffness=.75)
   assert int(kv(t.call("layer.rig.links",generation=epoch))["links"])==2
   graph=[kv(t.call("layer.rig.link.inspect",generation=epoch,
       revision=current(),index=i)) for i in range(2)]
   assert {(l["parent_id"],l["child_id"]) for l in graph}=={
       ("101","102"),("102","103")}
   assert not t.call("layer.rig.link",generation=epoch,revision=current(),
             parent_id=103,parent_vertex_id=c,
             child_id=101,child_vertex_id=a,
             stiffness=.5)["ok"],"THREE_LAYER_GRAPH_CYCLE_ACCEPTED"
   edit("layer.physics.bind",layer=third_index)
   for attempt in range(75):
    physical=kv(t.call("layer.physics.status",generation=epoch))
    if int(physical["accepted"])>=5 and physical["gpu"]=="1":break
    time.sleep(.06)
   assert int(physical.get("accepted","0"))>=5,"NO_THREE_LAYER_GPU_ACCEPTANCE"
   for layer_id,vertex_id in ((101,a),(102,b),(103,c)):
    frame=kv(t.call("layer.physics.point_by_id",generation=epoch,
                 id=layer_id,vertex_id=vertex_id))
    assert frame["gpu"]=="1" and int(frame["accepted"])>=5
   edit("layer.physics.stop")
   edit("layer.project.save")
   edit("layer.model.compile")
   edit("layer.rig.unlink",parent_id=102,parent_vertex_id=b,
                           child_id=103,child_vertex_id=c)
   edit("layer.rig.unlink",parent_id=101,parent_vertex_id=a,
                           child_id=102,child_vertex_id=b)
   assert int(kv(t.call("layer.rig.links",generation=epoch))["links"])==0
  restore=root/"restarted"
  restore.mkdir()
  shutil.copyfile(root/"session/layer_geometry.txt",restore/"layer_geometry.txt")
  shutil.copyfile(root/"session/model_runtime.armesh",restore/"model_runtime.armesh")
  with studio(restore,model) as (bridge,fresh,_):
   assert fresh!=epoch,"RESTART_EPOCH_UNCHANGED"
   t=AgentStudioTools(bridge)
   def revision():
    return int(kv(t.call("layer.project.status",generation=fresh))["revision"])
   assert kv(t.call("layer.project.status",generation=fresh))["layers"]=="0"
   assert kv(t.call("layer.project.load",generation=fresh,
                     revision=revision()))["links"]=="2"
   assert kv(t.call("layer.model.open",generation=fresh,
                     revision=revision()))["format"]=="ARS_MESH_RUNTIME_3"
   assert kv(t.call("layer.rig.links",generation=fresh))["links"]=="2"
   for i,(a_id,b_id) in enumerate((("101","102"),("102","103"))):
    link=kv(t.call("layer.rig.link.inspect",generation=fresh,
         revision=revision(),index=i))
    assert link["parent_id"]==a_id and link["child_id"]==b_id
   for id in (101,102,103):
    r=kv(t.call("layer.project.by_id",generation=fresh,
               revision=revision(),id=id))
    assert r["id"]==str(id)
   idx=int(kv(t.call("layer.project.by_id",generation=fresh,
            revision=revision(),id=103))["layer"])
   kv(t.call("layer.physics.bind",generation=fresh,
             revision=revision(),layer=idx))
   physics={}
   for attempt in range(75):
    physics=kv(t.call("layer.physics.status",generation=fresh))
    if int(physics["accepted"])>=5 and physics["gpu"]=="1":break
    time.sleep(.06)
   assert int(physics.get("accepted","0"))>=5,"RESTART_GRAPH_GPU_NOT_ACCEPTED"
   for id,vertex in ((101,a),(102,b),(103,c)):
    frame=kv(t.call("layer.physics.point_by_id",generation=fresh,
                    id=id,vertex_id=vertex))
    assert frame["gpu"]=="1"
   kv(t.call("layer.physics.stop",generation=fresh,revision=revision()))
  assert model.stat().st_mtime_ns==stamp,"SOURCE_MODEL_CHANGED"
  print("CROSS_RIG_NATIVE_PASS ids=1 reorder=1 stale=1 duplicate=1 deletion_guard=1 source_binary=1 gpu_both=1 gpu_three=1 cycle_guard=1 restart_graph=1 private=1")
  return 0
 finally:shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
