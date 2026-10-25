"""Private Native Studio authoring and physical GPU acceptance E2E."""
from __future__ import annotations
import time,shutil
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def values(receipt):
 return dict(entry.split("=",1) for entry in receipt.split(" ") if "=" in entry)
def call(tools,name,**kw):
 result=tools.call(name,**kw)
 assert result["ok"],"FAILED_"+name
 return values(result["receipt"])
def main():
 model=_private_manifest()
 if model is None or not model.is_file() or not EXE.is_file():
  print("GEOM_UV_PHYSICS_BLOCKED");return 3
 stamp=model.stat().st_mtime_ns
 root=ROOT/"build/studio-user"/("geom-uv-physics-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True,exist_ok=False)
 try:
  with studio(root/"session",model) as (bridge,epoch,_):
   t=AgentStudioTools(bridge)
   def edit(name,**kw):
    revision=int(call(t,"layer.project.status",generation=epoch)["revision"])
    return call(t,name,generation=epoch,revision=revision,**kw)
   edit("layer.create",id=79)
   for x,y in ((0.,0.),(2.,0.),(0.,2.)):
    edit("layer.point.add",layer=0,x=x,y=y)
   edit("layer.face.add",layer=0,a=0,b=1,c=2)
   edit("layer.uv.set",layer=0,vertex=1,u=.75,v=.4)
   edit("layer.material.set",layer=0,r=.2,g=.5,b=.9,opacity=.8)
   edit("layer.rig.pin",layer=0,vertex=0)
   edit("layer.rig.weight",layer=0,vertex=1,weight=.5)
   edit("layer.rig.parent",layer=0,vertex=2,parent=0,stiffness=.9)
   rigv=call(t,"layer.project.point",generation=epoch,layer=0,vertex=2)
   assert rigv["parent"]=="0" and abs(float(rigv["stiffness"])-.9)<1e-5
   rigw=call(t,"layer.project.point",generation=epoch,layer=0,vertex=1)
   assert abs(float(rigw["weight"])-.5)<1e-5
   compiled=edit("layer.model.compile")
   assert compiled["format"]=="ARS_MESH_RUNTIME_3"
   binary=root/"session/model_runtime.armesh"
   assert binary.is_file() and binary.stat().st_size>32
   edit("layer.model.open")
   rigv=call(t,"layer.project.point",generation=epoch,layer=0,vertex=2)
   rigw=call(t,"layer.project.point",generation=epoch,layer=0,vertex=1)
   assert rigv["parent"]=="0" and abs(float(rigw["weight"])-.5)<1e-5
   corrupted=bytearray(binary.read_bytes())
   corrupted[-1]^=0xff
   binary.write_bytes(corrupted)
   rev_before=int(call(t,"layer.project.status",generation=epoch)["revision"])
   assert not t.call("layer.model.open",generation=epoch,
                     revision=rev_before)["ok"],"CORRUPT_COMPILED_ACCEPTED"
   assert int(call(t,"layer.project.status",generation=epoch)["revision"])==rev_before
   edit("layer.model.compile")
   v=call(t,"layer.project.point",generation=epoch,layer=0,vertex=1)
   assert abs(float(v["u"])-.75)<1e-6 and abs(float(v["v"])-.4)<1e-6
   mat=call(t,"layer.project.layer",generation=epoch,layer=0)
   assert mat["pin"]=="0" and abs(float(mat["alpha"])-.8)<1e-6
   edit("layer.project.save")
   edit("layer.physics.bind",layer=0)
   status={}
   deadline=time.monotonic()+9
   while time.monotonic()<deadline:
    status=call(t,"layer.physics.status",generation=epoch)
    if int(status["accepted"])>=4 and status["gpu"]=="1":break
    time.sleep(.08)
   assert int(status.get("accepted","0"))>=4,"NO_GPU_ACCEPTED_PHYSICS_FRAMES"
   assert status["g5_applied"]=="0","CLAIMED_G5_APPLIED"
   time.sleep(.40)
   moved=call(t,"layer.physics.point",generation=epoch,vertex=2)
   assert abs(float(moved["y"])-2.)>1e-5,"XPBD_NO_VERTEX_MOVEMENT"
   pinned=call(t,"layer.physics.point",generation=epoch,vertex=0)
   assert abs(float(pinned["x"]))<1e-5 and abs(float(pinned["y"]))<1e-5,"PIN_DRIFT"

   edit("layer.material.set",layer=0,r=.9,g=.2,b=.1,opacity=.5)
   stale=call(t,"layer.physics.status",generation=epoch)
   assert stale["gpu"]=="0","STALE_GPU_FRAME_RETAINED"
   edit("layer.physics.bind",layer=0)
   edit("layer.physics.stop")
   assert call(t,"layer.physics.status",generation=epoch)["gpu"]=="0"
  reloaded=root/"reloaded";reloaded.mkdir()
  shutil.copyfile(root/"session/layer_geometry.txt",reloaded/"layer_geometry.txt")
  shutil.copyfile(root/"session/model_runtime.armesh",reloaded/"model_runtime.armesh")
  with studio(reloaded,model) as (bridge,new_epoch,_):
   tools=AgentStudioTools(bridge)
   rv=int(call(tools,"layer.project.status",generation=new_epoch)["revision"])
   loaded=call(tools,"layer.project.load",generation=new_epoch,revision=rv)
   loaded_runtime=call(tools,"layer.model.open",generation=new_epoch,
      revision=int(call(tools,"layer.project.status",generation=new_epoch)["revision"]))
   assert loaded_runtime["format"]=="ARS_MESH_RUNTIME_3"
   material=call(tools,"layer.project.layer",generation=new_epoch,layer=0)
   uv=call(tools,"layer.project.point",generation=new_epoch,layer=0,vertex=1)
   assert abs(float(material["alpha"])-.8)<1e-6,"MATERIAL_RELOAD"
   assert material["pin"]=="0","PIN_RELOAD"
   assert abs(float(uv["u"])-.75)<1e-6,"UV_RELOAD"
   assert abs(float(uv["weight"])-.5)<1e-6 and uv["parent"]=="-1"
   child=call(tools,"layer.project.point",generation=new_epoch,layer=0,vertex=2)
   assert child["parent"]=="0"

  assert model.stat().st_mtime_ns==stamp
  print("GEOM_UV_PHYSICS_PASS uv=1 material=1 source=1 rig_pin=1 xpbd_native=1 gpu_accepted=1 stale_fence=1")
  return 0
 finally:shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
