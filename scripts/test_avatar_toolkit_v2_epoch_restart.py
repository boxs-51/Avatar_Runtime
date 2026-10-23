"""Windows native two-process restart check for local opaque model generation.

No model ID, generation value, path, numeric payload or screenshot printed.
All private logs stay under the Git-ignored build/studio-user directory.
"""
import subprocess,sys,time,re
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge

def one_session(folder, prior=None):
 (folder/"api").mkdir(parents=True)
 exe=ROOT/"build/avatar-studio/Release/avatar_studio.exe"
 with (folder/"native-private.log").open("wb") as log:
  process=subprocess.Popen([str(exe),"--model",str(_private_manifest()),
     "--evidence",str(folder),"--frames","2000"],
     cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
  try:
   client=AgentBridge(folder/"api",timeout=8)
   result=client.call({"op":"studio.epoch"})
   if not result["ok"]:raise AssertionError("no epoch")
   if not re.fullmatch(r"generation=[1-9][0-9]{0,18}",result["receipt"]):
    raise AssertionError("bad epoch receipt")
   epoch=int(result["receipt"].split("=")[1])
   if prior is not None:
    assert epoch!=prior
    assert client.call({"op":"param.inspect.at","index":0,
                       "generation":prior})["ok"] is False
   assert client.call({"op":"param.inspect.at","index":0,
                       "generation":epoch})["ok"] is True
   return epoch
  finally:
   if process.poll() is None:
    process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait()

def main():
 folder=ROOT/"build/studio-user"/("epoch-restart-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 try:
  earlier=one_session(folder/"first")
  one_session(folder/"second",earlier)
 except Exception as exc:
  print("EPOCH_RESTART", "FAIL",type(exc).__name__)
  return 1
 print("EPOCH_RESTART","PASS","same_model_two_processes_old_generation_denied")
 return 0
if __name__=="__main__":raise SystemExit(main())
