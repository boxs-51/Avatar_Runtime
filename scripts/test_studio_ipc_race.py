"""Windows physical multi-process IPC admission regression (synthetic, no model)."""
import sys,subprocess,time
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from studio_agent_api import AgentBridge

def worker(folder,index):
 gate=folder/"go"
 deadline=time.monotonic()+6
 while not gate.exists() and time.monotonic()<deadline:time.sleep(.005)
 state="OTHER"
 try:AgentBridge(folder/"api",timeout=.4).call({"op":"studio.status"})
 except TimeoutError:state="TIMEOUT"
 except RuntimeError:state="BUSY"
 (folder/("result"+index+".txt")).write_text(state,encoding="ascii")
 return 0
def main():
 folder=ROOT/"build/studio-user"/("ipc-race-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 (folder/"api").mkdir(parents=True)
 procs=[subprocess.Popen([sys.executable,__file__,"--worker",str(folder),str(i)],
          stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,cwd=ROOT) for i in range(2)]
 try:
  time.sleep(.3)
  (folder/"go").write_text("go",encoding="ascii")
  for p in procs:p.wait(timeout=8)
  result=[(folder/("result"+str(i)+".txt")).read_text(encoding="ascii") for i in range(2)]
  correct=sorted(result)==["BUSY","TIMEOUT"]
  safe=not (folder/"api"/"request.txt").exists() and (folder/"api"/"writer.lock").is_file()
  print("IPC_TWO_PROCESS_GATE", "PASS" if correct and safe else "FAIL")
  return 0 if correct and safe else 1
 finally:
  for p in procs:
   if p.poll() is None:p.kill();p.wait()
if __name__=="__main__":
 raise SystemExit(worker(Path(sys.argv[2]),sys.argv[3]) if len(sys.argv)>1 and sys.argv[1]=="--worker" else main())
