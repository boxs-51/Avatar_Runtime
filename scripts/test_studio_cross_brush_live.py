"""Cross-drawable joint and local weight brush, private model remains on Host."""
import json,sys,subprocess,re,os
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge

def main():
 session=ROOT/"build/studio-user"/("cross-brush-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 (session/"api").mkdir(parents=True)
 exe=Path(os.environ.get("AVATAR_STUDIO_E2E_EXE",
  str(ROOT/"build/avatar-studio/Release/avatar_studio.exe")))
 assert exe.is_file()
 gates=[]
 with (session/"private-native.log").open("wb") as stream:
  proc=subprocess.Popen([str(exe),"--model",str(_private_manifest()),
             "--evidence",str(session),"--frames","2600"],
             cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
  try:
   bridge=AgentBridge(session/"api",timeout=9)
   def call(body,ok=True):
    reply=bridge.call(body)
    if reply["ok"]!=ok:
     raise RuntimeError(body["op"]+" receipt unexpected")
    return reply["receipt"]
   call({"op":"rig.select","drawable":0})
   call({"op":"rig.vertex","index":0})
   assert "touched=" in call({"op":"rig.brush","radius":.5,"weight":.25})
   assert "weight=0.25" in call({"op":"rig.weight.inspect","vertex":0})
   call({"op":"rig.store","action":"save"})
   call({"op":"rig.weight","vertex":0,"weight":1})
   call({"op":"rig.store","action":"load"})
   assert "weight=0.25" in call({"op":"rig.weight.inspect","vertex":0})
   gates.append("weight_brush_v4_save_load")
   call({"op":"rig.cross","target":1,"parent":1,"child":1,"stiffness":.8,"enabled":True})
   assert "cross=1 target=1" in call({"op":"rig.cross.inspect"})
   call({"op":"rig.cross","target":1,"parent":0,"child":1,"stiffness":.7,"enabled":True},False)
   call({"op":"rig.cross","target":0,"parent":1,"child":1,"stiffness":.7,"enabled":True},False)
   call({"op":"rig.store","action":"save"})
   call({"op":"rig.cross","target":1,"parent":1,"child":1,"stiffness":.8,"enabled":False})
   assert "cross=0" in call({"op":"rig.cross.inspect"})
   call({"op":"rig.store","action":"load"})
   assert "cross=1 target=1" in call({"op":"rig.cross.inspect"})
   gates.append("cross_drawable_parent_v4")
   call({"op":"rig.apply"})
   assert "xpbd=1" in call({"op":"studio.status"})
   call({"op":"studio.metrics"})
   gates.append("cross_drawable_xpbd_bind")
   status="PASS"
  except Exception as exc:
   status="FAIL"
   gates.append("error:"+type(exc).__name__+":"+str(exc)[:80])
  finally:
   if proc.poll() is None:
    proc.terminate()
    try:proc.wait(timeout=5)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
 (session/"summary.json").write_text(json.dumps({"status":status,"gates":gates},indent=2),encoding="utf-8")
 print("CROSS_BRUSH_E2E",status)
 for gate in gates:print("GATE",gate)
 return 0 if status=="PASS" else 1
if __name__=="__main__":raise SystemExit(main())
