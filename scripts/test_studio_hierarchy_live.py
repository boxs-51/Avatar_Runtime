"""Private fixture E2E for hierarchical rig and cross-island suggestions."""
import json,re,subprocess,sys,time
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge

def run():
 folder=ROOT/"build/studio-user"/("hierarchy-e2e-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 (folder/"api").mkdir(parents=True)
 exe=ROOT/"build/avatar-studio/Release/avatar_studio.exe"
 reports=[]
 with (folder/"studio-private.log").open("wb") as raw_log:
  process=subprocess.Popen([str(exe),"--model",str(_private_manifest()),
                 "--evidence",str(folder),"--frames","2200"],
                 cwd=ROOT,stdout=raw_log,stderr=subprocess.STDOUT)
  try:
   b=AgentBridge(folder/"api",timeout=8)
   def call(command,expected=True):
    r=b.call(command)
    if r["ok"]!=expected:
     raise AssertionError("Native receipt did not match "+command["op"])
    return r["receipt"]
   call({"op":"rig.select","drawable":0})
   call({"op":"rig.parent","parent":0,"child":2,"stiffness":.85,"enabled":True})
   call({"op":"rig.parent","parent":2,"child":4,"stiffness":.75,"enabled":True})
   call({"op":"rig.parent","parent":4,"child":0,"stiffness":.8,"enabled":True},False)
   call({"op":"rig.parent","parent":0,"child":4,"stiffness":.8,"enabled":True},False)
   assert "parent=0" in call({"op":"rig.parent.inspect","child":2})
   assert "parent=-1" in call({"op":"rig.parent.inspect","child":5})
   call({"op":"rig.weight","vertex":3,"weight":.25})
   state=call({"op":"rig.inspect"})
   assert "parents=2" in state and "weights=1" in state
   call({"op":"rig.store","action":"save"})
   call({"op":"rig.weight","vertex":3,"weight":1})
   assert "weight=1" in call({"op":"rig.weight.inspect","vertex":3})
   call({"op":"rig.store","action":"load"})
   assert "weight=0.25" in call({"op":"rig.weight.inspect","vertex":3})
   assert "parents=2" in call({"op":"rig.inspect"})
   assert "parent=2" in call({"op":"rig.parent.inspect","child":4})
   call({"op":"rig.apply"})
   call({"op":"studio.metrics"})
   reports.append({"gate":"hierarchy_roundtrip","ok":True,
                   "parent_count":2,"weight_roundtrip":True,"cycles_rejected":True})
   scan=call({"op":"layer.topology.scan"})
   result=dict(re.findall(r"([a-z_]+)=(\d+)",scan))
   assert int(result["multi"])>0
   relation_found=False
   for i in range(min(int(result["total"]),350)):
    selection=b.call({"op":"rig.select","drawable":i})
    if not selection["ok"]:continue
    top=b.call({"op":"layer.topology"})
    if not top["ok"]:continue
    attrs=dict(re.findall(r"([a-z_]+)=(\d+)",top["receipt"]))
    if int(attrs.get("islands",0))<2:continue
    receipt=call({"op":"layer.component.relation","first":0,"second":1})
    assert "nearest_a=" in receipt and "nearest_b=" in receipt and "gap=" in receipt
    # Looking at proximity never edits or activates a physical constraint.
    assert "parents=0" in call({"op":"rig.inspect"})
    relation_found=True
    break
   assert relation_found,"No multi-component drawable could be inspected"
   reports.append({"gate":"mesh_relation_nonmutating","ok":True,"candidate_found":True})
   status="PASS"
  except Exception as exc:
   status="FAIL"
   reports.append({"gate":"failure","category":type(exc).__name__})
  finally:
   if process.poll() is None:
    process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait()
 (folder/"summary.json").write_text(json.dumps({"status":status,"gates":reports},indent=2),
                                     encoding="utf-8")
 print("HIERARCHY_MODEL_E2E",status)
 for gate in reports:print("GATE",gate["gate"],gate.get("ok",False))
 return 0 if status=="PASS" else 1
if __name__=="__main__":raise SystemExit(run())
