"""Read-only model layer resolution diagnostic; private assets stay on Host."""
import json,re,subprocess,sys,time
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge

def main():
 model=_private_manifest()
 assert model.is_file(),"Local fixture missing"
 folder=ROOT/"build/studio-user"/("layer-resolution-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 (folder/"api").mkdir(parents=True)
 exe=ROOT/"build/avatar-studio/Release/avatar_studio.exe"
 cases=[]
 with (folder/"native.log").open("wb") as log:
  p=subprocess.Popen([str(exe),"--model",str(model),"--evidence",str(folder),
                      "--frames","2200"],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
  try:
   bridge=AgentBridge(folder/"api",timeout=8)
   ready=bridge.call({"op":"rig.select","drawable":0})
   if not ready["ok"]:raise RuntimeError("Studio readiness failed")
   t0=time.perf_counter()
   aggregate_top=bridge.call({"op":"layer.topology.scan"})
   if not aggregate_top["ok"]:
    raise RuntimeError("Topology aggregate scan rejected")
   top_counts=dict(re.findall(r"([a-z_]+)=(\d+)",aggregate_top["receipt"]))
   if int(top_counts["analyzed"])+int(top_counts["invalid"])!=int(top_counts["total"]):
    raise RuntimeError("Topology aggregate accounting mismatch")
   cases.append({"op":"layer.topology.scan","ok":True,"aggregate":top_counts})
   component=bridge.call({"op":"layer.component.inspect","component":0})
   if not component["ok"] or "anchor=" not in component["receipt"]:
    raise RuntimeError("Topology component detail not available")
   before=bridge.call({"op":"rig.inspect"})
   chosen=bridge.call({"op":"layer.component.adopt","component":0})
   after=bridge.call({"op":"rig.inspect"})
   if not all(r["ok"] for r in (before,chosen,after)):
    raise RuntimeError("Explicit component anchor failed")
   count=lambda receipt:int(re.search(r"anchors=(\d+)",receipt).group(1))
   if count(after["receipt"])<=count(before["receipt"]):
    raise RuntimeError("Component adoption did not change rig")
   invalid_component=bridge.call({"op":"layer.component.inspect","component":2000})
   if invalid_component["ok"]:
    raise RuntimeError("Invalid component accepted")
   cases.append({"op":"layer.component.inspect+adopt","ok":True,
                 "explicit_anchor_added":True,"invalid_rejected":True})
   top=bridge.call({"op":"layer.topology"})
   if not top["ok"] or "islands=" not in top["receipt"]:
    raise RuntimeError("Native topology query was rejected")
   top_numbers=dict(re.findall(r"([a-z]+)=(\d+)",top["receipt"]))
   if int(top_numbers["islands"])<1 or int(top_numbers["triangles"])<1:
    raise RuntimeError("Invalid topology aggregates")
   cases.append({"op":"layer.topology","ok":True,"aggregate":top_numbers})
   scan=bridge.call({"op":"layer.scan"})
   scan_ms=round((time.perf_counter()-t0)*1000,2)
   if not scan["ok"]:raise RuntimeError("layer.scan failed (no private labels logged)")
   matches=dict(re.findall(r"([a-z]+)=(\d+)",scan["receipt"]))
   assert sum(int(matches.get(x,0)) for x in ("hair","skirt","strap","unknown"))==int(matches["total"])
   cases.append({"op":"layer.scan","ok":True,"aggregate":matches,"latency_ms":scan_ms})
   chosen=0;known=False;unknown_checked=False
   for d in range(min(int(matches["total"]),24)):
    selected=bridge.call({"op":"rig.select","drawable":d})
    if not selected["ok"]:continue
    proposed=bridge.call({"op":"layer.suggest"})
    if not proposed["ok"]:continue
    attrs=dict(re.findall(r"([a-z]+)=([a-z0-9]+)",proposed["receipt"]))
    if attrs.get("kind")=="unknown" and not unknown_checked:
     rejected=bridge.call({"op":"layer.adopt"})
     assert not rejected["ok"],"Unknown proposal must not auto-rig"
     unknown_checked=True
    if attrs.get("kind") in ("hair","skirt","strap") and not known:
     adopted=bridge.call({"op":"layer.adopt"})
     assert adopted["ok"],"Explicit adoption failed for high confidence proposal"
     inspect=bridge.call({"op":"rig.inspect"})
     assert inspect["ok"] and "anchors=" in inspect["receipt"]
     known=True;chosen=d
    if known and unknown_checked:break
   chosen_role=False
   first=bridge.call({"op":"rig.select","drawable":0})
   if first["ok"]:
    labeled=bridge.call({"op":"layer.label","kind":"skirt"})
    assert labeled["ok"],"Manual role was rejected"
    read=bridge.call({"op":"rig.inspect"})
    assert read["ok"] and "role=skirt" in read["receipt"]
    other=bridge.call({"op":"rig.select","drawable":1})
    if other["ok"]:
     back=bridge.call({"op":"rig.select","drawable":0})
     again=bridge.call({"op":"rig.inspect"})
     assert back["ok"] and again["ok"] and "role=skirt" in again["receipt"]
    chosen_role=True
   cases.append({"op":"layer.label","ok":True,"persisted_in_rig_bank":chosen_role})
   cases.append({"op":"layer.suggest+adopt","ok":True,"recognized_tested":known,
                 "unknown_rejected":unknown_checked})
   result="PASS"
  except Exception as exc:
   cases.append({"ok":False,"reason":str(exc)})
   result="FAIL"
  finally:
   if p.poll() is None:
    p.terminate()
    try:p.wait(timeout=6)
    except subprocess.TimeoutExpired:p.kill();p.wait()
 summary={"result":result,"cases":cases}
 (folder/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
 print("LAYER_MODEL_TEST",result)
 for case in cases:
  if case.get("op")=="layer.scan":print("AGGREGATE",case["aggregate"])
  if case.get("op")=="layer.suggest+adopt":print("ADOPTION",case["recognized_tested"],
    "UNKNOWN_GUARD",case["unknown_rejected"])
  if not case.get("ok"):print("REASON",case.get("reason"))
 print("LOCAL_REPORT",folder.relative_to(ROOT))
 return 0 if result=="PASS" else 1
if __name__=="__main__":raise SystemExit(main())
