"""Native Windows G4 UI/Agent interleaving oracle. Local-only, never export model data."""
from __future__ import annotations
import json, pathlib, re, secrets, subprocess, sys, threading, time
from datetime import datetime
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
sys.path.insert(0,str(ROOT/"scripts"))
from studio_agent_api import AgentBridge
from moc3_local_replay import _private_manifest
from scripts.avatar_toolkit_v2 import execute_request

def touch(folder,name):
    (folder/name).write_text("1",encoding="ascii")
def read_json(folder,name):
    return json.loads((folder/name).read_text(encoding="utf-8"))
def write_json(folder,name,doc):
    path=folder/name
    path.write_text(json.dumps(doc,separators=(",",":")),encoding="utf-8")

def req(op,args,epoch):
    data={"api_version":2,"request_id":"req-g4-ui-race","session_id":None,
      "expected_model_generation":epoch,"expected_edit_revision":None,
      "operation":op,"arguments":args,"mode":"read"}
    return json.dumps(data).encode("ascii")

def inspect(bridge,epoch,d):
    meta=bridge.call({"op":"g4.rig.meta","generation":epoch,"drawable":d})
    if not meta["ok"]:raise RuntimeError("META_DENIED")
    fields=dict(t.split("=",1) for t in meta["receipt"].split(" "))
    n=int(fields["anchors"])
    if n>4:raise RuntimeError("BAD_FIXTURE")
    entries=[]
    for i in range(n):
        item=bridge.call({"op":"g4.rig.entry","generation":epoch,
                          "drawable":d,"kind":"anchor","index":i})
        if not item["ok"]:raise RuntimeError("ENTRY_DENIED")
        parts=dict(t.split("=",1) for t in item["receipt"].split(" "))
        entries.append(int(parts["vertex"]))
    return entries

class PausedBridge:
    def __init__(self,real,folder):
        self.real=real;self.folder=folder
    def call(self,cmd):
        if cmd["op"]=="g4.rig.entry" and cmd["kind"]=="anchor" and cmd["index"]==1:
            touch(self.folder,"agent_paused.flag")
            deadline=time.monotonic()+100
            while not (self.folder/"resume.flag").exists():
                if time.monotonic()>deadline:raise TimeoutError("UI_BARRIER_TIMEOUT")
                time.sleep(.015)
        return self.real.call(cmd)

def worker(folder):
    model=_private_manifest()
    exe=ROOT/"build/avatar-studio/Release/avatar_studio.exe"
    if model is None or not model.is_file() or not exe.is_file():
        write_json(folder,"result.json",{"verdict":"BLOCKED","reason":"FIXTURE_OR_BIN_MISSING"})
        return 3
    (folder/"api").mkdir(exist_ok=False)
    with (folder/"native-private.log").open("xb") as log:
        proc=subprocess.Popen([str(exe),"--model",str(model),"--frames","18000",
            "--evidence",str(folder)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+15
            while not (folder/"state.json").exists():
                if proc.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError("NATIVE_START_FAILED")
                time.sleep(.05)
            bridge=AgentBridge(folder/"api",timeout=5)
            epoch_reply=bridge.call({"op":"studio.epoch"})
            if not epoch_reply["ok"]:raise RuntimeError("NO_EPOCH")
            epoch=int(epoch_reply["receipt"].split("=",1)[1])
            status=bridge.call({"op":"studio.status"})
            drawables=int(re.search(r"\bdrawables=(\d+)",status["receipt"]).group(1))
            d=None
            for candidate in range(min(drawables,128)):
                meta=bridge.call({"op":"g4.mesh.meta","generation":epoch,
                                  "drawable":candidate})
                if not meta["ok"]:continue
                nums=dict(t.split("=",1) for t in meta["receipt"].split(" "))
                if int(nums["vertices"])>=3:
                    if bridge.call({"op":"rig.select","drawable":candidate})["ok"]:
                        d=candidate;break
            if d is None:raise RuntimeError("NO_ELIGIBLE_DRAWABLE")
            for index in (0,1):
                if not bridge.call({"op":"rig.anchor","vertex":index,
                                    "enabled":True})["ok"]:
                    raise RuntimeError("SETUP_ANCHOR_FAILED")
            if inspect(bridge,epoch,d)!=[0,1]:
                raise RuntimeError("PRECONDITION_NOT_MET")
            before=inspect(bridge,epoch,d)
            aba=(folder/"actor_aba.flag").exists()
            agent_actor=aba or (folder/"actor_agent.flag").exists()
            def revision():
                reply=bridge.call({"op":"g4.rig.revision","generation":epoch,"drawable":d})
                if not reply["ok"]:raise RuntimeError("REVISION_DENIED")
                return int(reply["receipt"].split("=",1)[1])
            pre_revision=revision()
            observed={}
            def read():
                try:
                    observed["result"]=json.loads(execute_request(
                        req("rig.entries.page",{"drawable":d,"kind":"anchor",
                           "start":0,"limit":2},epoch),
                        local_bridge=PausedBridge(bridge,folder),
                        allow_local_private=True))
                except Exception as exc:
                    observed["exception"]=type(exc).__name__
            t=threading.Thread(target=read,daemon=True);t.start()
            deadline=time.monotonic()+12
            while not (folder/"agent_paused.flag").exists():
                if time.monotonic()>deadline:raise RuntimeError("READ_BARRIER_NOT_REACHED")
                time.sleep(.025)
            write_json(folder,"ready.json",{"pid":proc.pid,"drawable":d,"phase":("AGENT_IPC_EDIT" if (folder/"actor_agent.flag").exists() else "UI_EDIT")})
            if agent_actor:
                if aba:
                    if not bridge.call({"op":"rig.anchor","vertex":2,"enabled":True})["ok"]:
                        raise RuntimeError("ABA_ADD_FAILED")
                    middle=inspect(bridge,epoch,d)
                    middle_revision=revision()
                    if middle!=[0,1,2] or middle_revision<=pre_revision:
                        raise RuntimeError("ABA_MIDDLE_NOT_PROVEN")
                    if not bridge.call({"op":"rig.anchor","vertex":2,"enabled":False})["ok"]:
                        raise RuntimeError("ABA_REMOVE_FAILED")
                else:
                    if not bridge.call({"op":"rig.anchor","vertex":0,"enabled":False})["ok"]:
                        raise RuntimeError("ACTOR_REMOVE_FAILED")
                    if not bridge.call({"op":"rig.anchor","vertex":2,"enabled":True})["ok"]:
                        raise RuntimeError("ACTOR_ADD_FAILED")
                touch(folder,"verify_and_resume.flag")
            deadline=time.monotonic()+95
            while not (folder/"verify_and_resume.flag").exists():
                if time.monotonic()>deadline:raise RuntimeError("UI_ACTION_TIMEOUT")
                if proc.poll() is not None:raise RuntimeError("NATIVE_PREMATURE_EXIT")
                time.sleep(.025)
            after=inspect(bridge,epoch,d)
            post_revision=revision()
            edit_verified=(after==[0,1] and post_revision>pre_revision
                           if aba else after==[1,2])
            if aba and (post_revision<=middle_revision or after!=before):
                raise RuntimeError("ABA_RETURN_NOT_PROVEN")
            proof={"pre":[0,1],"post_is_expected":edit_verified,
                   "post_count":len(after),
                   "revision_increased":post_revision>pre_revision}
            write_json(folder,"ui-proof.json",proof)
            touch(folder,"resume.flag")
            t.join(timeout=10)
            if t.is_alive():raise RuntimeError("AGENT_THREAD_TIMEOUT")
            result=observed.get("result")
            if result is None:
                verdict="HARNESS_FAILURE"
            elif result["state"]=="COMPLETED_READ":
                items=[x["vertex"] for x in result["result"]["items"]]
                verdict=("SEMANTIC_RED_ABA_ACCEPTED" if aba and edit_verified else
                    "SEMANTIC_RED_MIXED_SNAPSHOT"
                    if before==[0,1] and after==[1,2] and items==[0,2]
                    else "CONTROL_OR_UNEXPECTED_SUCCESS")
            elif result["state"]=="REJECTED":
                if aba:
                    verdict=("REJECTED_ABA_STALE_REVISION" if edit_verified and
                             result["error"]["code"]=="STALE_REVISION" else
                             "ABA_UNEXPECTED_REJECTION")
                else:
                    verdict=("REJECTED_AFTER_EDITOR_CHANGE" if after==[1,2] and
                             result["error"]["code"]=="STALE_REVISION" else
                             "UNVERIFIED_UI_CHANGED")
            else:verdict="UNEXPECTED_STATE"
            write_json(folder,"result.json",
                {"verdict":verdict,"actor":"AGENT_IPC_ABA" if aba else
                    "AGENT_IPC" if agent_actor else "UI_DESKTOP",
                 "edit_verified":edit_verified,
                 "revision_increased":post_revision>pre_revision,
                 "native_process":True,"agent_finished":True,
                 "api_state":result["state"] if result else "EXCEPTION",
                 "error_code":result["error"]["code"] if result and result["error"]
                    else None})
            print(("G4_NATIVE_ABA=" if aba else
                   "G4_NATIVE_AGENT_RACE=" if agent_actor else
                   "G4_UI_RACE=")+verdict,flush=True)
            return 0 if verdict in ("REJECTED_AFTER_EDITOR_CHANGE",
                                    "REJECTED_ABA_STALE_REVISION") else 2
        except Exception as exc:
            write_json(folder,"result.json",
                {"verdict":"HARNESS_OR_ENV_FAILURE",
                 "reason_class":type(exc).__name__})
            print("G4_UI_RACE=HARNESS_OR_ENV_FAILURE",flush=True)
            return 3
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:proc.wait(timeout=7)
                except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=7)

def begin():
    base=ROOT/"build/studio-user"
    base.mkdir(parents=True,exist_ok=True)
    folder=base/("g4-ui-race-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    folder.mkdir(exist_ok=False)
    with (folder/"runner-private.log").open("wb") as log:
        p=subprocess.Popen([sys.executable,str(pathlib.Path(__file__).resolve()),
          "--worker",str(folder)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
          creationflags=getattr(subprocess,"CREATE_NEW_PROCESS_GROUP",0))
    print(json.dumps({"runner_pid":p.pid,"case_dir":str(folder.relative_to(ROOT))}))
    return 0

def probe(folder):
    if (folder/"result.json").exists():
        data=read_json(folder,"result.json")
        print("STATE=FINISHED verdict="+data["verdict"])
    elif (folder/"ready.json").exists():
        data=read_json(folder,"ready.json")
        print("STATE=UI_EDIT pid="+str(data["pid"]))
    else:
        print("STATE=STARTING")
    return 0

def finish(folder):
    touch(folder,"verify_and_resume.flag")
    deadline=time.monotonic()+18
    while time.monotonic()<deadline and not (folder/"result.json").exists():
        time.sleep(.025)
    if not (folder/"result.json").exists():
        print("STATE=NO_FINAL_RESULT")
        return 3
    data=read_json(folder,"result.json")
    print("STATE=FINISHED verdict="+data["verdict"]+
          " actor="+data.get("actor","UNKNOWN")+" edit_verified="+str(data.get("edit_verified",False)))
    return 0

if __name__=="__main__":
    mode=sys.argv[1]
    if mode=="--begin":sys.exit(begin())
    if mode in ("--run-agent","--run-aba"):
        aba=mode=="--run-aba"
        prefix="g4-agent-aba-" if aba else "g4-agent-race-"
        folder=ROOT/"build/studio-user"/(prefix+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
        folder.mkdir(parents=True,exist_ok=False)
        touch(folder,"actor_aba.flag" if aba else "actor_agent.flag")
        sys.exit(worker(folder))
    if len(sys.argv)!=3:sys.exit(2)
    folder=pathlib.Path(sys.argv[2]).resolve()
    if not folder.is_relative_to((ROOT/"build/studio-user").resolve()):
        sys.exit(2)
    if mode=="--worker":sys.exit(worker(folder))
    if mode=="--probe":sys.exit(probe(folder))
    if mode=="--finish":sys.exit(finish(folder))
    sys.exit(2)
