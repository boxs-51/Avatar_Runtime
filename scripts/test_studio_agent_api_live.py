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
                {"op":"rig.select","drawable":1},
                {"op":"rig.select","drawable":0},
                {"op":"rig.inspect"},
                {"op":"physics.set","gravity":-8,"wind":2,"damping":.94},
                {"op":"action.trigger","kind":"pull","strength":.5},
                {"op":"action.trigger","kind":"release","strength":0},
                {"op":"rig.apply"},
                {"op":"studio.status"},
                {"op":"studio.play","enabled":False},
                {"op":"studio.step"},
                {"op":"studio.expression","index":5},
                {"op":"studio.toggle","feature":"blink","enabled":False},
                {"op":"studio.setting","name":"motion_speed","value":1.2},
                {"op":"studio.camera","mode":"fit"},
                {"op":"studio.zoom","value":1.4},
                {"op":"studio.pan","x":5,"y":-3},
                {"op":"param.clear","index":0},
                {"op":"param.reset"},
                {"op":"part.opacity","index":0,"value":.8},
                {"op":"rig.vertex","index":2},
                {"op":"rig.store","action":"save"},
                {"op":"rig.store","action":"load"},
                {"op":"xpbd.set","name":"bending","value":.001},
                {"op":"xpbd.set","name":"substeps","value":4},
                {"op":"xpbd.select","first":0,"second":-1,"joint":False},
                {"op":"xpbd.inspect"},
                {"op":"studio.metrics"},
                {"op":"xpbd.apply"},
                {"op":"xpbd.disable"},
                {"op":"studio.preset","action":"save"},
                {"op":"studio.capture","kind":"viewport"},
                {"op":"studio.play","enabled":True},
                {"op":"studio.tab","index":3},
                {"op":"studio.background","index":2},
                {"op":"param.inspect","index":0},
                {"op":"part.inspect","index":0},
                {"op":"xpbd.read","name":"bending"},
                {"op":"studio.status"},
                {"op":"studio.preset","action":"load"},
                {"op":"studio.camera","mode":"face"},
            ]
            for command in commands:
                if process.poll() is not None:raise RuntimeError("Studio exited during API test")
                result=bridge.call(command)
                cases.append({"op":command["op"],**result})
                if not result["ok"]:raise RuntimeError("API rejected "+command["op"]+": "+result["receipt"])
                if command["op"]=="xpbd.read" and "field=bending value=" not in result["receipt"]:
                    raise RuntimeError("XPBD readback missing")
                if command["op"]=="part.inspect" and "opacity=0.8" not in result["receipt"]:
                    raise RuntimeError("Part opacity did not apply")
                if command["op"]=="studio.preset" and "success=1" not in result["receipt"]:
                    raise RuntimeError("Preset was not verified")
                if command["op"]=="rig.inspect" and not ("anchors=2" in result["receipt"] and "beams=1" in result["receipt"]):
                    raise RuntimeError("Rig state lost across drawable switch")
            # Bypass the Python validator deliberately; Native must still reject
            # malformed commands. Protocol payloads contain no model identifiers.
            denied=["STUDIO_CAPTURE desktop","XPBD_SET gravity nan",
                    "PARAM_SET -1 0","RIG_STORE load ../../outside",
                    "STUDIO_PLAY 2","STUDIO_STATUS extra",
                    "UNKNOWN_EXEC powershell"]
            for index,wire in enumerate(denied):
                token=f"{index+1:024x}"
                (base/"api"/"request.txt").write_text(token+" "+wire+"\n",encoding="ascii")
                deadline=time.monotonic()+3
                answer=None
                while time.monotonic()<deadline:
                    response=base/"api"/"response.txt"
                    if response.exists():
                        payload=response.read_text(encoding="ascii").strip()
                        if payload.startswith(token+" "):
                            answer=payload
                            response.unlink()
                            break
                    time.sleep(.02)
                if answer is None or not answer.startswith(token+" ERR "):
                    raise RuntimeError("Native input gate accepted malformed command")
            print("NATIVE_REJECT_MATRIX_PASS count="+str(len(denied)))
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
