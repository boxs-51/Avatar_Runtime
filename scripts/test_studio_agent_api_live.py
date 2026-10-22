"""Physical local Agent API test using the real Native Studio; no data leaves Host."""
from __future__ import annotations
import json
from pathlib import Path
import subprocess
import sys
import time
from datetime import datetime

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from studio_agent_api import AgentBridge
from moc3_local_replay import _private_manifest

def main()->int:
    model=_private_manifest()
    if not model or not model.is_file():raise RuntimeError("Local model fixture absent")
    base=ROOT/"build/studio-user"/("agent-api-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    (base/"api").mkdir(parents=True)
    binary=ROOT/"build/avatar-studio/Release/avatar_studio.exe"
    with (base/"native.log").open("wb") as log:
        process=subprocess.Popen([str(binary),"--model",str(model),"--frames","1300",
            "--evidence",str(base)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        cases=[]
        try:
            deadline=time.monotonic()+12
            while time.monotonic()<deadline and not (base/"state.json").exists():
                if process.poll() is not None:raise RuntimeError("Studio failed before API started")
                time.sleep(.05)
            bridge=AgentBridge(base/"api",timeout=5)
            commands=[
                {"op":"rig.select","drawable":0},
                {"op":"rig.anchor","vertex":0,"enabled":True},
                {"op":"rig.anchor","vertex":1,"enabled":True},
                {"op":"rig.beam","a":1,"b":2,"enabled":True},
                {"op":"physics.set","gravity":-8,"wind":2,"damping":.94},
                {"op":"action.trigger","kind":"pull","strength":.5},
                {"op":"action.trigger","kind":"release","strength":0},
                {"op":"rig.apply"},
            ]
            for command in commands:
                if process.poll() is not None:raise RuntimeError("Studio exited during API test")
                result=bridge.call(command)
                cases.append({"op":command["op"],**result})
                if not result["ok"]:raise RuntimeError("API rejected "+command["op"]+": "+result["receipt"])
            print("NATIVE_AGENT_API_PASS commands="+str(len(cases)))
        finally:
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=8)
                except subprocess.TimeoutExpired:process.kill();process.wait()
        (base/"summary.json").write_text(json.dumps({"cases":cases,"result":"PASS"},indent=2),encoding="utf-8")
        print("LOCAL_REPORT="+str(base.relative_to(ROOT)))
        return 0
if __name__=="__main__":raise SystemExit(main())
