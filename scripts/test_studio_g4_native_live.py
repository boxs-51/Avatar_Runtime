"""Physical Windows Studio G4 read E2E. Private receipts never leave ignored test session."""
from __future__ import annotations
import json
import pathlib
import re
import secrets
import subprocess
import sys
import time
from datetime import datetime
from contextlib import contextmanager

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge
from scripts.avatar_toolkit_v2 import execute_request

EXE=ROOT/"build/avatar-studio/Release/avatar_studio.exe"
EXPECTED=("mesh.topology.inspect","mesh.vertices.page","mesh.triangles.page",
          "rig.local.inspect","rig.entries.page")
CASES=[]

def check(ok,code):
    if not ok:raise AssertionError(code)
    CASES.append(code)

def local(bridge,op,args,epoch):
    wire=json.dumps({"api_version":2,"request_id":"req-g4-e2e",
      "session_id":None,"expected_model_generation":epoch,
      "expected_edit_revision":None,"operation":op,
      "arguments":args,"mode":"read"},separators=(",",":")).encode()
    return json.loads(execute_request(wire,local_bridge=bridge,allow_local_private=True))

def native_raw(folder,cmd,timeout=4):
    api=folder/"api"
    token=secrets.token_hex(12)
    with (api/"request.txt").open("x",encoding="ascii") as handle:
        handle.write(token+" "+cmd+"\n")
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:
            p=api/"response.txt"
            if p.exists():
                raw=p.read_text("ascii")
                if raw.startswith(token+" ") and raw.endswith("\n") and raw.count("\n")==1:
                    parts=raw[:-1].split(" ",2)
                    if len(parts)==3 and parts[1] in ("OK","ERR") and parts[2]:
                        p.unlink()
                        return parts[1]
        except (OSError,UnicodeError):
            pass
        time.sleep(.012)
    raise AssertionError("RAW_ACK_TIMEOUT")

@contextmanager
def studio(folder,model):
    (folder/"api").mkdir(parents=True,exist_ok=False)
    with (folder/"native-private.log").open("xb") as log:
        process=subprocess.Popen([str(EXE),"--model",str(model),
          "--evidence",str(folder),"--frames","7200"],cwd=ROOT,
          stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+12
            while time.monotonic()<deadline:
                if process.poll() is not None:raise AssertionError("NATIVE_EARLY_EXIT")
                if (folder/"state.json").exists():break
                time.sleep(.05)
            else:raise AssertionError("NATIVE_NO_READY")
            bridge=AgentBridge(folder/"api",timeout=5)
            epoch_msg=bridge.call({"op":"studio.epoch"})
            check(epoch_msg["ok"],"EPOCH_AVAILABLE")
            epoch=int(epoch_msg["receipt"].split("=",1)[1])
            msg=bridge.call({"op":"studio.status"})
            check(msg["ok"],"STATUS_HEALTHY")
            fields=dict(re.findall(r"([a-z]+)=([0-9]+)",msg["receipt"]))
            drawable=None
            for d in range(min(int(fields["drawables"]),128)):
                meta=bridge.call({"op":"g4.mesh.meta","generation":epoch,"drawable":d})
                if meta["ok"]:
                    selected=bridge.call({"op":"rig.select","drawable":d})
                    if selected["ok"]:
                        drawable=d
                        break
            if drawable is None:raise AssertionError("NO_ELIGIBLE_RIG_DRAWABLE")
            check(True,"DRAWABLE_SELECTED")
            yield bridge,epoch,drawable
        finally:
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=6)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=6)

def exercise(bridge,epoch,d,folder,prefix):
    anchor=bridge.call({"op":"rig.anchor","vertex":0,"enabled":True})
    check(anchor["ok"],prefix+"_ANCHOR_MEMORY_ONLY")
    requests=(
      ("mesh.topology.inspect",{"drawable":d}),
      ("mesh.vertices.page",{"drawable":d,"start":0,"limit":1}),
      ("mesh.triangles.page",{"drawable":d,"start":0,"limit":1}),
      ("rig.local.inspect",{"drawable":d}),
      ("rig.entries.page",{"drawable":d,"kind":"anchor","start":0,"limit":1}))
    for op,args in requests:
        result=local(bridge,op,args,epoch)
        check(result["state"]=="COMPLETED_READ" and
              result["result"]["classification"]=="LOCAL_PRIVATE" and
              result["result"]["model_generation"]==epoch,
              prefix+"_SUCCESS_"+op)
    malformed=(
       f"G4_MESH_META {epoch} {d} suffix",
       f"G4_MESH_VERTEX {epoch} {d} nan",
       f"G4_MESH_TRI {epoch} {d} -1",
       f"G4_RIG_META {epoch} {d} suffix",
       f"G4_RIG_ENTRY {epoch} {d} anchor -1")
    for op,wire in zip(EXPECTED,malformed):
        check(native_raw(folder,wire)=="ERR",prefix+"_MALFORMED_"+op)
    # Negative commands must not poison a subsequent ordinary read.
    check(local(bridge,"rig.local.inspect",{"drawable":d},epoch)["state"]=="COMPLETED_READ",
          prefix+"_POST_NEGATIVE_HEALTH")

def main():
    model=_private_manifest()
    if model is None or not model.is_file() or not EXE.is_file():
        print("G4_NATIVE_E2E=BLOCKED missing_local_fixture_or_binary")
        return 3
    root=ROOT/"build/studio-user"/("g4-native-e2e-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    root.mkdir(parents=True,exist_ok=False)
    try:
        with studio(root/"session-a",model) as (bridge,old,d):
            exercise(bridge,old,d,root/"session-a","SESSION_A")
        with studio(root/"session-b",model) as (bridge,new,d):
            check(old!=new,"RESTART_NEW_EPOCH")
            for op,wire in (
              ("mesh.topology.inspect",f"G4_MESH_META {old} {d}"),
              ("mesh.vertices.page",f"G4_MESH_VERTEX {old} {d} 0"),
              ("mesh.triangles.page",f"G4_MESH_TRI {old} {d} 0"),
              ("rig.local.inspect",f"G4_RIG_META {old} {d}"),
              ("rig.entries.page",f"G4_RIG_ENTRY {old} {d} anchor 0")):
                check(native_raw(root/"session-b",wire)=="ERR","RESTART_STALE_NATIVE_"+op)
            stale=local(bridge,"mesh.topology.inspect",{"drawable":d},old)
            check(stale["state"]=="REJECTED" and stale["error"]["code"]=="STALE_MODEL",
                  "RESTART_STALE_V2")
            exercise(bridge,new,d,root/"session-b","SESSION_B")
        (root/"public-safe-summary.json").write_text(json.dumps(
          {"result":"PASS","cases":[{"case_id":code,"result":"PASS"} for code in CASES]},
           indent=2),encoding="utf-8")
        print("G4_NATIVE_E2E=PASS cases="+str(len(CASES))+" sessions=2 raw_private_not_exported=true")
        return 0
    except Exception as exc:
        # Only stable, code-controlled case IDs are safe on stdout; no private exception details.
        label=str(exc)
        known={"NATIVE_EARLY_EXIT","NATIVE_NO_READY","RAW_ACK_TIMEOUT",
               "NO_ELIGIBLE_RIG_DRAWABLE","RESTART_NEW_EPOCH","RESTART_STALE_V2",
               "EPOCH_AVAILABLE","STATUS_HEALTHY","DRAWABLE_SELECTED"}
        for prefix in ("SESSION_A","SESSION_B"):
            known.update({prefix+"_ANCHOR_MEMORY_ONLY",prefix+"_POST_NEGATIVE_HEALTH"})
            known.update(prefix+"_SUCCESS_"+op for op in EXPECTED)
            known.update(prefix+"_MALFORMED_"+op for op in EXPECTED)
        known.update("RESTART_STALE_NATIVE_"+op for op in EXPECTED)
        if label not in known:
            label="UNCLASSIFIED_PRIVATE_ERROR"
        import traceback
        frames=traceback.extract_tb(exc.__traceback__)
        safe=[f.name for f in frames if f.name in ("exercise","local","read_geometry","_g4_metadata","_parse_receipt","_call","main")]
        print("G4_NATIVE_E2E=FAIL case="+label+" type="+type(exc).__name__+
              " location="+",".join(safe)+" passed="+str(len(CASES)))
        return 1

if __name__=="__main__":
    raise SystemExit(main())
