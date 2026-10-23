"""RED-first Native response sharing denial: publish after lock release.

Windows local private-model test. Prints only categorical verdicts.
All raw output stays under the ignored Studio user directory.
"""
import ctypes,os,subprocess,sys,time
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest
from studio_agent_api import AgentBridge

def main():
    root=ROOT/"build/studio-user"/("ipc-share-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    api=root/"api";api.mkdir(parents=True)
    exe=Path(os.environ.get("AVATAR_STUDIO_E2E_EXE",str(ROOT/"build/avatar-studio/Release/avatar_studio.exe")))
    dll=ctypes.WinDLL("kernel32",use_last_error=True)
    dll.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,
      ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
    dll.CreateFileW.restype=ctypes.c_void_p
    dll.CloseHandle.argtypes=[ctypes.c_void_p];dll.CloseHandle.restype=ctypes.c_int
    invalid=ctypes.c_void_p(-1).value
    with (root/"native-private.log").open("wb") as log:
      proc=subprocess.Popen([str(exe),"--model",str(_private_manifest()),
        "--evidence",str(root),"--frames","2800"],cwd=ROOT,
        stdout=log,stderr=subprocess.STDOUT)
      handle=None
      try:
        result=AgentBridge(api,timeout=8).call({"op":"studio.epoch"})
        assert result["ok"],"studio not responsive"
        reply=api/"response.txt"
        reply.write_text("000000000000000000000000 ERR sentinel\n",encoding="ascii")
        handle=dll.CreateFileW(str(reply),0x80000000,0x00000001,None,3,0x80,None)
        if handle in (None,invalid):raise RuntimeError("sharing lock unavailable")
        token="1234567890abcdef12345678"
        request=api/"request.txt"
        request.write_text(token+" STUDIO_EPOCH\n",encoding="ascii")
        time.sleep(.25)
        # A current unread response must prevent admitting a newer command.
        if not request.exists():
          print("IPC_UNREAD_ACK_BLOCK","FAIL");return 1
        if not reply.read_text(encoding="ascii").startswith("000000000000000000000000 "):
          print("IPC_UNREAD_ACK_BLOCK","FAIL");return 1
        print("IPC_UNREAD_ACK_BLOCK","PASS")
        dll.CloseHandle(handle);handle=None
        reply.unlink() # explicit acknowledgement of synthetic old response
        deadline=time.monotonic()+5
        first=False
        while time.monotonic()<deadline:
          try:
            if reply.exists():
              data=reply.read_text(encoding="ascii")
              if data.startswith(token+" OK ") and data.endswith("\n"):
                first=True;break
          except OSError:pass
          time.sleep(.015)
        if not first:
          print("IPC_FIRST_ACK","FAIL");return 1
        token_b="abcdef0123456789abcdef01"
        request.write_text(token_b+" STUDIO_EPOCH\n",encoding="ascii")
        time.sleep(.22)
        try:
          holds_first=reply.read_text(encoding="ascii").startswith(token+" OK ")
          b_waiting=request.exists()
        except OSError:
          holds_first=False;b_waiting=False
        if not (holds_first and b_waiting):
          print("IPC_ACK_OWNERSHIP","FAIL");return 1
        reply.unlink() # client acknowledges A; only now may B execute
        deadline=time.monotonic()+5
        second=False
        while time.monotonic()<deadline:
          try:
            if reply.exists():
              data=reply.read_text(encoding="ascii")
              if data.startswith(token_b+" OK ") and data.endswith("\n"):
                second=True;break
          except OSError:pass
          time.sleep(.015)
        print("IPC_ACK_OWNERSHIP","PASS" if second else "FAIL")
        return 0 if second else 1
      finally:
        if handle is not None:dll.CloseHandle(handle)
        if proc.poll() is None:
          proc.terminate()
          try:proc.wait(timeout=5)
          except subprocess.TimeoutExpired:proc.kill();proc.wait()

if __name__=="__main__":raise SystemExit(main())
