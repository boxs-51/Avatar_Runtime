"""Isolated real GUI interactions, PID/foreground/window-at-point fenced."""
import ctypes as ct,subprocess,shutil,time,re,sys
from ctypes import wintypes as w
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_api import AgentBridge
from scripts.studio_agent_tools import AgentStudioTools

u=ct.windll.user32
class Point(ct.Structure):_fields_=[("x",w.LONG),("y",w.LONG)]
class Rect(ct.Structure):_fields_=[("left",w.LONG),("top",w.LONG),("right",w.LONG),("bottom",w.LONG)]
class MouseInput(ct.Structure):
 _fields_=[("dx",w.LONG),("dy",w.LONG),("mouseData",w.DWORD),
           ("dwFlags",w.DWORD),("time",w.DWORD),("extra",ct.c_size_t)]
class InputUnion(ct.Union):
 _fields_=[("mi",MouseInput),("pad",ct.c_byte*40)]
class Input(ct.Structure):
 _fields_=[("type",w.DWORD),("data",InputUnion)]
def mouse_transition(flags):
 packet=Input();packet.type=0;packet.data.mi=MouseInput(0,0,0,flags,0,0)
 assert u.SendInput(1,ct.byref(packet),ct.sizeof(packet))==1,"SENDINPUT_BLOCKED"
EnumCB=ct.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
def window_by_pid(pid):
 matches=[]
 def scan(hwnd,_):
  actual=w.DWORD(0);u.GetWindowThreadProcessId(hwnd,ct.byref(actual))
  title=ct.create_unicode_buffer(256);u.GetWindowTextW(hwnd,title,256)
  if actual.value==pid and title.value=="Avatar Studio | Interactive Model Lab" and u.IsWindowVisible(hwnd):
   matches.append(hwnd)
  return True
 cb=EnumCB(scan);u.EnumWindows(cb,0)
 assert len(matches)==1,"TARGET_WINDOW_NOT_UNIQUE"
 return matches[0]
def click(hwnd,pid,x,y):
 assert u.IsWindow(hwnd),"WINDOW_GONE"
 actual=w.DWORD(0);u.GetWindowThreadProcessId(hwnd,ct.byref(actual))
 assert actual.value==pid,"WINDOW_PID_CHANGED"
 assert u.GetForegroundWindow()==hwnd,"WRONG_FOREGROUND"
 rect=Rect();assert u.GetClientRect(hwnd,ct.byref(rect))
 assert 0<=x<rect.right and 0<=y<rect.bottom,"OUTSIDE_TARGET"
 pt=Point(x,y);assert u.ClientToScreen(hwnd,ct.byref(pt))
 found=u.WindowFromPoint(pt)
 owner=w.DWORD(0);u.GetWindowThreadProcessId(found,ct.byref(owner))
 assert owner.value==pid,"WINDOW_AT_POINT_MISMATCH"
 assert u.SetCursorPos(pt.x,pt.y),"CURSOR_FAILED"
 time.sleep(.06)
 assert u.GetForegroundWindow()==hwnd,"FOREGROUND_CHANGED"
 actual=w.DWORD(0)
 u.GetWindowThreadProcessId(u.WindowFromPoint(pt),ct.byref(actual))
 assert actual.value==pid,"TARGET_CHANGED_BEFORE_INPUT"
 mouse_transition(0x0002);time.sleep(.13)
 mouse_transition(0x0004);time.sleep(.16)
def status(tools,epoch):
 result=tools.call("layer.project.status",generation=epoch)
 assert result["ok"],"GEOMETRY_STATUS_MISSING"
 return dict((k,int(v)) for k,v in re.findall(r"([a-z]+)=(-?[0-9]+)",result["receipt"]))

def main():
 model=_private_manifest()
 if model is None or not model.is_file() or not EXE.is_file():
  print("GUI_GEOMETRY_BLOCKED_FIXTURE");return 3
 root=ROOT/"build/studio-user"/("geom-gui-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
 root.mkdir(parents=True,exist_ok=False)
 folder=root/"trial";(folder/"api").mkdir(parents=True)
 process=None
 u.GetForegroundWindow.restype=w.HWND
 u.WindowFromPoint.argtypes=[Point];u.WindowFromPoint.restype=w.HWND
 u.SetForegroundWindow.argtypes=[w.HWND]
 u.GetClientRect.argtypes=[w.HWND,ct.POINTER(Rect)]
 u.ClientToScreen.argtypes=[w.HWND,ct.POINTER(Point)]
 u.GetWindowThreadProcessId.argtypes=[w.HWND,ct.POINTER(w.DWORD)]
 prior=u.GetForegroundWindow()
 try:
  with (folder/"private-native.log").open("xb") as log:
   process=subprocess.Popen([str(EXE),"--model",str(model),
          "--evidence",str(folder),"--frames","7200"],cwd=ROOT,
          stdout=log,stderr=subprocess.STDOUT)
   deadline=time.monotonic()+14
   while time.monotonic()<deadline and not (folder/"state.json").is_file():
    assert process.poll() is None,"NATIVE_EARLY_EXIT"
    time.sleep(.08)
   assert (folder/"state.json").is_file(),"NATIVE_NOT_READY"
   bridge=AgentBridge(folder/"api",timeout=5)
   reply=bridge.call({"op":"studio.epoch"})
   assert reply["ok"],"NO_MODEL_EPOCH"
   epoch=int(reply["receipt"].split("=",1)[1])
   tools=AgentStudioTools(bridge)
   before=status(tools,epoch)
   hwnd=window_by_pid(process.pid)
   assert u.ShowWindow(hwnd,9) is not None
   assert u.MoveWindow(hwnd,0,0,1250,920,True),"RESIZE_FAIL"
   u.SetForegroundWindow(hwnd);time.sleep(.18)
   if "--bridge-focus" in sys.argv and u.GetForegroundWindow()!=hwnd:
    handoff=ROOT/"build/studio-user"/"geometry-gui-focus-handoff.txt"
    handoff.write_text(str(process.pid),encoding="ascii")
    deadline=time.monotonic()+45
    while time.monotonic()<deadline and u.GetForegroundWindow()!=hwnd:
     assert process.poll() is None,"STUDIO_EXITED"
     time.sleep(.10)
    handoff.unlink(missing_ok=True)
   assert u.GetForegroundWindow()==hwnd,"FOCUS_DENIED"
   rect=Rect();u.GetClientRect(hwnd,ct.byref(rect))
   assert rect.right>=1250 and rect.bottom>=900,"INSUFFICIENT_CLIENT_SIZE"
   x=rect.right-316
   if "--pause-debug" in sys.argv:time.sleep(20)
   time.sleep(.6)
   for retry in range(4):
    click(hwnd,process.pid,x+106,150)
    ui_state=tools.call("studio.status")
    if ui_state["ok"] and "tab=1" in ui_state["receipt"]:break
    time.sleep(.30)
   ui_state=tools.call("studio.status")
   tab_click_ok=bool(ui_state["ok"] and "tab=1" in ui_state["receipt"])
   if not tab_click_ok:
    switched=tools.call("studio.tab",index=1)
    assert switched["ok"],"LAYER_TAB_FALLBACK_DENIED"
    assert "tab=1" in tools.call("studio.status")["receipt"],"LAYER_TAB_NOT_ACTIVE"
   click(hwnd,process.pid,x+65,478)   # Create custom layer
   assert status(tools,epoch)["layers"]==before["layers"]+1,"GUI_CREATE_FAILED"
   click(hwnd,process.pid,x+65,573)   # Enable add-point clicks
   for vx,vy in ((420,340),(540,340),(420,420)):
    click(hwnd,process.pid,vx,vy)
   assert status(tools,epoch)["revision"]==before["revision"]+4,"GUI_POINT_ADD_FAILED"
   click(hwnd,process.pid,x+60,606)   # Add last triangle
   assert status(tools,epoch)["revision"]==before["revision"]+5,"GUI_FACE_FAILED"
   click(hwnd,process.pid,x+215,475)  # Delete custom layer
   assert status(tools,epoch)["layers"]==before["layers"],"GUI_DELETE_FAILED"
   click(hwnd,process.pid,x+60,708)   # Undo deletion
   assert status(tools,epoch)["layers"]==before["layers"]+1,"GUI_UNDO_FAILED"
   click(hwnd,process.pid,x+65,747)   # Save private sidecar
   assert (folder/"layer_geometry.txt").is_file(),"GUI_SAVE_FAILED"
   print("GUI_GEOMETRY_PASS pid_fenced=1 target_window=1 tab_mouse="+str(int(tab_click_ok))+" create=1 point_clicks=3 triangle=1 delete=1 undo=1 save=1")
   return 0
 except Exception as e:
  print("GUI_GEOMETRY_BLOCKED_OR_FAIL code="+
        (str(e) if isinstance(e,AssertionError) and
         str(e).isupper() and len(str(e))<64 else type(e).__name__))
  return 1
 finally:
  if process is not None and process.poll() is None:
   process.terminate()
   try:process.wait(timeout=6)
   except subprocess.TimeoutExpired:process.kill();process.wait(timeout=6)
  if prior and u.IsWindow(prior):u.SetForegroundWindow(prior)
  if "--bridge-focus" in sys.argv:
   (ROOT/"build/studio-user"/"geometry-gui-focus-handoff.txt").unlink(missing_ok=True)
  shutil.rmtree(root,ignore_errors=True)
if __name__=="__main__":raise SystemExit(main())
