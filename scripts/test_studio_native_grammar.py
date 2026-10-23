"""Native parser suffix security oracle. Local-only disposable Studio session."""
import os,sys,time,subprocess,ast
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge

def main():
 base=ROOT/"build/studio-user"/("f01-native-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 api=base/"api";api.mkdir(parents=True)
 binary=Path(os.environ.get("AVATAR_STUDIO_E2E_EXE",str(ROOT/"build/avatar-studio/RelWithDebInfo/avatar_studio.exe")))
 with (base/"native-private.log").open("wb") as log:
  proc=subprocess.Popen([str(binary),"--model",str(_private_manifest()),
    "--evidence",str(base),"--frames","2200"],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
  failures=[]
  try:
   bridge=AgentBridge(api,timeout=6)
   bridge.call({"op":"rig.select","drawable":1})
   def raw(wire,index):
    token=f"{index:024x}"
    (api/"request.txt").write_text(token+" "+wire+"\n",encoding="ascii")
    end=time.monotonic()+4
    while time.monotonic()<end:
     path=api/"response.txt"
     if path.exists():
      data=path.read_text(encoding="ascii")
      if data.startswith(token+" "):
       path.unlink();return data.split()[1]
     time.sleep(.015)
    return "TIMEOUT"
   cases=[
    ("RIG_SELECT 0 invalid",lambda: "drawable=1" in bridge.call({"op":"rig.inspect"})["receipt"]),
    ("RIG_WEIGHT 3 .25 invalid",lambda:"weights=0" in bridge.call({"op":"rig.inspect"})["receipt"]),
    ("RIG_PARENT 0 2 .8 1 invalid",lambda:"parents=0" in bridge.call({"op":"rig.inspect"})["receipt"]),
    ("RIG_ANCHOR 0 1 invalid",lambda:"anchors=0" in bridge.call({"op":"rig.inspect"})["receipt"]),
    ("LAYER_LABEL hair invalid",lambda:"role=unknown" in bridge.call({"op":"rig.inspect"})["receipt"]),
    ("RIG_APPLY invalid",lambda:"xpbd=0" in bridge.call({"op":"studio.status"})["receipt"]),
    ("LAYER_SCAN invalid",lambda:True),
    ("PHYSICS_SET -8 1 .95 invalid",lambda:True),
   ]
   module=ast.parse((ROOT/"tests/moc3/test_studio_agent_api.py").read_text(encoding="utf-8-sig"))
   for cls in module.body:
    if not isinstance(cls,ast.ClassDef):continue
    for method in cls.body:
     if not isinstance(method,ast.FunctionDef) or method.name!="test_extended_studio_api":continue
     for statement in method.body:
      if isinstance(statement,ast.Assign) and any(
          isinstance(name,ast.Name) and name.id=="cases" for name in statement.targets):
       catalog=ast.literal_eval(statement.value)
       for _,wire in catalog:
        cases.append((wire+" invalid",lambda:True))
   cases.extend([
       ("RIG_SELECT 0 123",lambda:True),
       ("RIG_SELECT 0 "+"X"*800,lambda:True),
       ("RIG_APPLY 1",lambda:True),
       ("STUDIO_STATUS 999",lambda:True),
   ])
   for i,(wire,check) in enumerate(cases,1):
    status=raw(wire,i)
    if status!="ERR" or not check():failures.append(f"F01-{i:02d}")
   print("F01_NATIVE_CASES",len(cases),"FAIL_COUNT",len(failures))
   for code in failures:print("RED_CASE",code)
   return 1 if failures else 0
  finally:
   if proc.poll() is None:
    proc.terminate()
    try:proc.wait(timeout=5)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
if __name__=="__main__":raise SystemExit(main())
