"""Private model E2E for ADR-002 local read-only V2 adapter, no asset export."""
import json,subprocess,sys,os
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge
from scripts.avatar_toolkit_v2 import execute_request

def msg(op,args=None,expected=None):
 return json.dumps({"api_version":2,"request_id":"req-physical-local-01",
 "session_id":None,"expected_model_generation":expected,"expected_edit_revision":None,
 "operation":op,"arguments":args or {},"mode":"read"}).encode("utf-8")

def run():
 folder=ROOT/"build/studio-user"/("v2-local-read-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 (folder/"api").mkdir(parents=True)
 exe=Path(os.environ.get("AVATAR_STUDIO_E2E_EXE",str(ROOT/"build/avatar-studio/Release/avatar_studio.exe")))
 gates=[]
 with (folder/"native-private.log").open("wb") as log:
  proc=subprocess.Popen([str(exe),"--model",str(_private_manifest()),
    "--evidence",str(folder),"--frames","1700"],
    cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
  try:
   bridge=AgentBridge(folder/"api",timeout=7)
   def read(op,args=None):
    r=json.loads(execute_request(msg(op,args),
               local_bridge=bridge,allow_local_private=True))
    if r["state"]!="COMPLETED_READ":raise RuntimeError("local read rejected")
    return r["result"]
   status=read("studio.local.status")
   assert status["counts"]["params"]>0 and status["counts"]["parts"]>0
   assert status["classification"]=="LOCAL_PRIVATE"
   assert status["generation_available"] is True
   generation=status["model_generation"]
   assert type(generation) is int and 0<generation<2**63
   current=bridge.call({"op":"param.inspect.at","generation":generation,"index":0})
   assert current["ok"]
   wrong=generation-1 if generation>1 else generation+1
   stale=bridge.call({"op":"param.inspect.at","generation":wrong,"index":0})
   assert not stale["ok"]
   fenced=json.loads(execute_request(msg("catalog.parameters.page",
        {"start":0,"limit":1},expected=generation),local_bridge=bridge,
        allow_local_private=True))
   assert fenced["state"]=="COMPLETED_READ"
   denied_stale=json.loads(execute_request(msg("catalog.parameters.page",
        {"start":0,"limit":1},expected=wrong),local_bridge=bridge,
        allow_local_private=True))
   assert denied_stale["state"]=="REJECTED"
   assert denied_stale["error"]["code"]=="STALE_MODEL"
   gates.append("native_generation_cas")

   gates.append("status_live")
   a=read("catalog.parameters.page",{"start":0,"limit":2})
   b=read("catalog.parts.page",{"start":0,"limit":2})
   assert len(a["items"])==2 and len(b["items"])==2
   assert all(item["index"]==i for i,item in enumerate(a["items"]))
   assert all(item["index"]==i for i,item in enumerate(b["items"]))
   gates.append("paged_native_parameter_part")
   denied=json.loads(execute_request(msg("catalog.parameters.page",{"start":0,"limit":1}),
                    local_bridge=bridge,allow_local_private=False))
   assert denied["state"]=="REJECTED"
   gates.append("private_denial")
   code=0
  except Exception as exc:
   gates.append("error_class:"+type(exc).__name__)
   code=1
  finally:
   if proc.poll() is None:
    proc.terminate()
    try:proc.wait(timeout=5)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
 print("LOCAL_READ_E2E","PASS" if code==0 else "FAIL")
 for item in gates:print("CASE",item)
 return code
if __name__=="__main__":raise SystemExit(run())
