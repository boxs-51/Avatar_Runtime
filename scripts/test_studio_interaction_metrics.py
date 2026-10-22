"""Live per-action observations; local numerical telemetry only."""
import json,subprocess,sys,time
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge

def main():
 model=_private_manifest()
 folder=ROOT/"build/studio-user"/("interactive-metrics-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 (folder/"api").mkdir(parents=True)
 log=(folder/"native.log").open("wb")
 p=subprocess.Popen([str(ROOT/"build/avatar-studio/Release/avatar_studio.exe"),"--model",str(model),"--evidence",str(folder),"--frames","1500"],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
 bridge=AgentBridge(folder/"api",timeout=5)
 stages=[
  ("rig",{"op":"rig.select","drawable":0}),
  ("anchor0",{"op":"rig.anchor","vertex":0,"enabled":True}),
  ("anchor1",{"op":"rig.anchor","vertex":1,"enabled":True}),
  ("beam",{"op":"rig.beam","a":1,"b":2,"enabled":True}),
  ("apply",{"op":"rig.apply"}),
  ("pull",{"op":"action.trigger","kind":"pull","strength":.9}),
  ("shake",{"op":"action.trigger","kind":"shake","strength":.7}),
  ("release",{"op":"action.trigger","kind":"release","strength":0}),
  ("settle",{"op":"action.trigger","kind":"settle","strength":0}),
 ]
 report=[]
 try:
  for label,cmd in stages:
   result=bridge.call(cmd)
   start=time.monotonic()
   while time.monotonic()-start<3:
    if p.poll() is not None:raise RuntimeError("Studio exited")
    file=folder/("state.json" if label in ("rig","anchor0","anchor1","beam") else "xpbd_metrics.json")
    try:
     data=json.loads(file.read_text(encoding="utf-8"))
     if data["frame"]>=15 and (label=="rig" or data["frame"]>report[-1]["frame"]+10):
      break
    except (OSError,ValueError,IndexError,KeyError):pass
    time.sleep(.03)
   else:
    raise RuntimeError("No fresh physics metrics for "+label)
   report.append({"stage":label,"receipt":result["receipt"],"ok":result["ok"],**data})
   print(label,"frame",data["frame"],"accepted",data.get("accepted_frames",0),
         "max_step",data.get("xpbd_max_step",0),"max_accel",data.get("xpbd_max_acceleration",0),
         "pin_drift",data.get("xpbd_pin_drift",0))
   if not result["ok"]:raise AssertionError(label)
  # Observe long settling, do not infer recovery from cumulative maxima.
  start_frame=report[-1]["frame"]
  deadline=time.monotonic()+10
  while time.monotonic()<deadline:
   if p.poll() is not None:raise RuntimeError("Studio exited before settling")
   try:
    tail=json.loads((folder/"xpbd_metrics.json").read_text(encoding="utf-8"))
   except (OSError,ValueError):time.sleep(.03);continue
   if tail["frame"]>=start_frame+160:break
   time.sleep(.03)
  else:raise RuntimeError("No long-settle observation")
  report.append({"stage":"settle_long","ok":True,**tail})
  print("settle_long frame",tail["frame"],"current_step",tail.get("xpbd_current_step"),"pin_drift",tail["xpbd_pin_drift"])
  baseline=next(item for item in report if item["stage"]=="apply")
  pulled=next(item for item in report if item["stage"]=="pull")
  rested=next(item for item in report if item["stage"]=="settle")
  recovered=report[-1]
  assert pulled["xpbd_max_step"]>baseline["xpbd_max_step"]*2, "No response to pull"
  assert recovered["xpbd_current_step"]<rested["xpbd_current_step"]*.4, "Motion did not settle"
  assert recovered["interaction_pull_x"]==0, "Interaction force persisted after release"
  assert report[-1]["accepted_frames"]>20
  assert report[-1]["xpbd_pin_drift"]<1e-5
  assert report[-1]["xpbd_max_step"]>0
  summary={"result":"PASS","stages":report}
 except Exception as exc:
  summary={"result":"FAIL","reason":str(exc),"stages":report}
 finally:
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait()
  log.close()
 (folder/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
 print("RESULT",summary["result"],"LOCAL_REPORT",folder.relative_to(ROOT))
 return 0 if summary["result"]=="PASS" else 1
if __name__=="__main__":raise SystemExit(main())
