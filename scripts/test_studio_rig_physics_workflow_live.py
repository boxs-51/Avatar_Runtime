"""Isolated physical Rig/XPBD E2E; never publish local model data."""
import re,shutil,time
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def call(t,op,**kwargs):
 r=t.call(op,**kwargs)
 assert r["ok"],"REJECT_"+op
 return r["receipt"]
def rev(t,g):
 value=call(t,"rig.history",generation=g)
 m=re.fullmatch(r"revision=([0-9]+) undo=([0-9]+) redo=([0-9]+)",value)
 assert m,"BAD_HISTORY"
 return tuple(map(int,m.groups()))
def frame(t,expected):
 for _ in range(100):
  v=t.legacy_frame_status()
  assert not v["g5_applied"],"G5_APPLIED"
  if v["legacy_frame_current"]:
   assert v["legacy_rig_revision"]==expected
   assert v["accepted_rig_revision"]==expected
   return v["accepted_frame_serial"]
  time.sleep(.05)
 raise AssertionError("FRAME_NOT_ACCEPTED")
def main():
 model=_private_manifest()
 if model is None or not model.is_file() or not EXE.is_file():
  print("RIG_PHYSICS_E2E_BLOCKED");return 3
 folder=ROOT/"build/studio-user"/("rig-workflow-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 folder.mkdir(parents=True,exist_ok=False)
 try:
  with studio(folder/"session",model) as (bridge,generation,drawable):
   t=AgentStudioTools(bridge);initial=rev(t,generation)
   assert not t.legacy_frame_status()["legacy_frame_current"]
   call(t,"rig.anchor",vertex=0,enabled=True)
   call(t,"rig.beam",a=0,b=1,enabled=True)
   call(t,"rig.parent",parent=0,child=2,stiffness=.8,enabled=True)
   call(t,"rig.weight",vertex=2,weight=.25)
   stage=rev(t,generation)
   assert stage[0]==initial[0]+4 and stage[1]>=4
   assert "weight=0.25" in call(t,"rig.weight.inspect",vertex=2)
   call(t,"rig.apply");a=frame(t,stage[0])
   assert "active=1" in call(t,"xpbd.inspect")
   call(t,"rig.undo",generation=generation,revision=stage[0])
   undo=rev(t,generation)
   assert undo[0]==stage[0]+1 and undo[2]==1
   assert not t.legacy_frame_status()["legacy_frame_current"]
   assert "weight=1" in call(t,"rig.weight.inspect",vertex=2)
   call(t,"rig.apply");b=frame(t,undo[0])
   assert b>a
   call(t,"rig.redo",generation=generation,revision=undo[0])
   redo=rev(t,generation)
   assert redo[0]==undo[0]+1
   assert not t.legacy_frame_status()["legacy_frame_current"]
   assert "weight=0.25" in call(t,"rig.weight.inspect",vertex=2)
   call(t,"rig.apply");c=frame(t,redo[0])
   assert c>b
   call(t,"xpbd.apply")
   assert not t.legacy_frame_status()["legacy_frame_current"],"GENERIC_XPBD_RETAINED_RIG_FRAME"
   call(t,"rig.apply")
   d=frame(t,redo[0]);assert d>c
   call(t,"xpbd.disable")
   assert not t.legacy_frame_status()["legacy_frame_current"],"DISABLED_SOLVER_HAS_STALE_FRAME"
   assert "active=0" in call(t,"xpbd.inspect")
   assert not t.g5_status()["g5_writer_enabled"]
  print("RIG_PHYSICS_E2E_PASS ops=4 undo_redo=1 physical_frames=4 writer=HOLD model_private=1")
  return 0
 finally:shutil.rmtree(folder,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
