"""Local-only agent command API for Native Studio. No network or model export."""
from __future__ import annotations
import math
import os
from pathlib import Path
import secrets
import re
import time
VERSION="1.0"
def validate_command(cmd:dict)->str:
    if not isinstance(cmd,dict) or not isinstance(cmd.get("op"),str):
        raise ValueError("invalid operation")
    op=cmd["op"]
    fields={
        "studio.status":{"op"}, "studio.metrics":{"op"},
        "studio.epoch":{"op"},
        "param.id.at":{"op","generation","index"},
        "part.id.at":{"op","generation","index"},
        "param.find.at":{"op","generation","id"},
        "part.find.at":{"op","generation","id"},
        "param.inspect.at":{"op","generation","index"}, "part.inspect.at":{"op","generation","index"},
        "studio.tab":{"op","index"}, "studio.background":{"op","index"},
        "param.inspect":{"op","index"}, "part.inspect":{"op","index"},
        "xpbd.read":{"op","name"},
        "studio.play":{"op","enabled"}, "studio.step":{"op"},
        "studio.expression":{"op","index"},
        "studio.toggle":{"op","feature","enabled"},
        "studio.setting":{"op","name","value"},
        "studio.camera":{"op","mode"}, "studio.zoom":{"op","value"},
        "studio.pan":{"op","x","y"}, "studio.reset":{"op"},
        "studio.preset":{"op","action"}, "studio.capture":{"op","kind"},
        "param.set":{"op","index","value"}, "param.clear":{"op","index"},
        "param.reset":{"op"}, "part.opacity":{"op","index","value"},
        "rig.vertex":{"op","index"}, "rig.store":{"op","action"},
        "xpbd.set":{"op","name","value"},
        "xpbd.select":{"op","first","second","joint"},
        "xpbd.apply":{"op"}, "xpbd.disable":{"op"}, "xpbd.inspect":{"op"},
        "layer.topology":{"op"}, "layer.topology.scan":{"op"},
        "layer.component.inspect":{"op","component"},
        "layer.component.adopt":{"op","component"},
        "rig.select":{"op","drawable"},
        "rig.anchor":{"op","vertex","enabled"},
        "rig.beam":{"op","a","b","enabled"},
        "rig.parent":{"op","parent","child","stiffness","enabled"},
        "rig.parent.inspect":{"op","child"},
        "rig.cross":{"op","target","parent","child","stiffness","enabled"},
        "rig.cross.inspect":{"op"},
        "rig.brush":{"op","radius","weight"},
        "rig.weight":{"op","vertex","weight"},
        "rig.weight.inspect":{"op","vertex"},
        "layer.component.relation":{"op","first","second"},
        "rig.apply":{"op"},
        "rig.inspect":{"op"},
        "layer.scan":{"op"},
        "layer.suggest":{"op"},
        "layer.adopt":{"op"},
        "layer.label":{"op","kind"},
        "physics.set":{"op","gravity","wind","damping"},
        "action.trigger":{"op","kind","strength"},
    }
    if op not in fields or set(cmd)!=fields[op]:raise ValueError("invalid fields")
    def integer(k):
        v=cmd[k]
        if type(v) is not int or not 0<=v<=2047:raise ValueError("index out of bounds")
        return v
    def finite(k,low,high):
        v=cmd[k]
        if type(v) not in (float,int) or not math.isfinite(v) or not low<=v<=high:
            raise ValueError("invalid scalar")
        return format(v,".9g")
    def boolean(k):
        if type(cmd[k]) is not bool:raise ValueError("expected bool")
        return int(cmd[k])
    singles={"studio.epoch":"STUDIO_EPOCH","studio.status":"STUDIO_STATUS","studio.step":"STUDIO_STEP",
             "studio.reset":"STUDIO_RESET","studio.metrics":"STUDIO_METRICS",
             "param.reset":"PARAM_RESET","xpbd.apply":"XPBD_APPLY",
             "xpbd.disable":"XPBD_DISABLE","xpbd.inspect":"XPBD_INSPECT"}
    if op in singles:return singles[op]
    if op in ("param.id.at","part.id.at","param.find.at","part.find.at"):
        gen=cmd["generation"]
        if type(gen) is not int or not 0<gen<2**63:
            raise ValueError("invalid generation")
        prefix="PARAM_" if op.startswith("param.") else "PART_"
        if op.endswith("id.at"):
            return f"{prefix}ID_AT {gen} {integer('index')}"
        value=cmd["id"]
        if type(value) is not str or re.fullmatch(r"[A-Za-z0-9_.:-]{1,59}",value,re.ASCII) is None:
            raise ValueError("invalid model ID")
        return f"{prefix}FIND_AT {gen} {value}"
    if op in ("param.inspect.at","part.inspect.at"):
        generation=cmd["generation"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid model generation")
        index=integer("index")
        return ("PARAM_INSPECT_AT" if op=="param.inspect.at"
                else "PART_INSPECT_AT")+f" {generation} {index}"
    if op in ("studio.tab","studio.background"):
        index=integer("index")
        if index>3:raise ValueError("invalid UI index")
        return ("STUDIO_TAB" if op=="studio.tab" else "STUDIO_BACKGROUND")+f" {index}"
    if op in ("param.inspect","part.inspect"):
        return ("PARAM_INSPECT" if op=="param.inspect" else "PART_INSPECT")+f" {integer('index')}"
    if op=="studio.play":return f"STUDIO_PLAY {boolean('enabled')}"
    if op=="studio.expression":
        i=integer("index")
        if i>8:raise ValueError("invalid expression")
        return f"STUDIO_EXPRESSION {i}"
    if op=="studio.toggle":
        name=cmd["feature"]
        if type(name) is not str or name not in ("physics","blink","breath","gaze",
             "speech","cycle","grid","particles"):raise ValueError("toggle")
        return f"STUDIO_TOGGLE {name} {boolean('enabled')}"
    settings={"motion_speed":(.1,2.5),"motion_amount":(0,1.5),
              "wind_drive":(0,1),"blink_interval":(1,8),
              "cycle_interval":(3,20)}
    if op=="studio.setting":
        name=cmd["name"]
        if type(name) is not str or name not in settings:raise ValueError("setting")
        return f"STUDIO_SETTING {name} {finite('value',*settings[name])}"
    if op=="studio.camera":
        if cmd["mode"] not in ("fit","face"):raise ValueError("camera")
        return "STUDIO_CAMERA "+cmd["mode"]
    if op=="studio.zoom":return "STUDIO_ZOOM "+finite("value",.3,3)
    if op=="studio.pan":
        return "STUDIO_PAN "+finite("x",-10000,10000)+" "+finite("y",-10000,10000)
    if op in ("studio.preset","rig.store"):
        if cmd["action"] not in ("save","load"):raise ValueError("action")
        return ("STUDIO_PRESET" if op=="studio.preset" else "RIG_STORE")+" "+cmd["action"]
    if op=="studio.capture":
        if cmd["kind"]!="viewport":raise ValueError("capture")
        return "STUDIO_CAPTURE viewport"
    if op=="param.set":return f"PARAM_SET {integer('index')} {finite('value',-1000,1000)}"
    if op=="param.clear":return f"PARAM_CLEAR {integer('index')}"
    if op=="part.opacity":return f"PART_OPACITY {integer('index')} {finite('value',0,1)}"
    if op=="rig.vertex":return f"RIG_VERTEX {integer('index')}"
    xpbd={"gravity":(-80,80),"wind":(-60,60),"damping":(0,1),
          "edge_compliance":(0,.0002),"collision":(0,.15),"mass":(.1,10),
          "contact_compliance":(0,.001),"iterations":(1,24),
          "step":(1/240,1/60),"pin":(0,100),"substeps":(1,8),
          "pose":(0,.02),"transfer":(0,1),"bending":(0,.002),
          "surface":(0,10),"anchors":(1,8)}
    if op=="xpbd.read":
        name=cmd["name"]
        if type(name) is not str or name not in xpbd:raise ValueError("unknown XPBD field")
        return "XPBD_READ "+name
    if op=="xpbd.set":
        name=cmd["name"]
        if type(name) is not str or name not in xpbd:raise ValueError("xpbd name")
        value=cmd["value"]
        if name in ("iterations","pin","substeps","anchors"):
            if type(value) is not int or not xpbd[name][0]<=value<=xpbd[name][1]:
                raise ValueError("xpbd integer")
            return f"XPBD_SET {name} {value}"
        return f"XPBD_SET {name} {finite('value',*xpbd[name])}"
    if op=="xpbd.select":
        a,b=integer("first"),cmd["second"]
        if type(b) is not int or not -1<=b<=2047 or b==a:
            raise ValueError("second mesh")
        joint=boolean("joint")
        if joint and b<0:raise ValueError("joint needs second mesh")
        return f"XPBD_SELECT {a} {b} {joint}"
    if op in ("layer.component.inspect","layer.component.adopt"):
        return ("LAYER_COMPONENT_INSPECT" if op.endswith("inspect") else "LAYER_COMPONENT_ADOPT")+f" {integer('component')}"
    if op=="layer.topology":return "LAYER_TOPOLOGY"
    if op=="layer.topology.scan":return "LAYER_TOPOLOGY_SCAN"
    if op=="rig.select":return f"RIG_SELECT {integer('drawable')}"
    if op=="rig.anchor":
        v=integer("vertex")
        if type(cmd["enabled"]) is not bool:raise ValueError("enabled must be bool")
        return f"RIG_ANCHOR {v} {int(cmd['enabled'])}"
    if op=="rig.beam":
        a,b=integer("a"),integer("b")
        if a==b or type(cmd["enabled"]) is not bool:raise ValueError("invalid beam")
        return f"RIG_BEAM {a} {b} {int(cmd['enabled'])}"
    if op=="rig.cross.inspect":return "RIG_CROSS_INSPECT"
    if op=="rig.cross":
        parent,child,target=integer("parent"),integer("child"),integer("target")
        if type(cmd["enabled"]) is not bool:raise ValueError("cross enabled bool")
        return f"RIG_CROSS {target} {parent} {child} {finite('stiffness',0,1)} {int(cmd['enabled'])}"
    if op=="rig.brush":
        return "RIG_BRUSH "+finite("radius",.00001,1000)+" "+finite("weight",0,1)
    if op=="rig.parent.inspect":
        return f"RIG_PARENT_INSPECT {integer('child')}"
    if op=="rig.parent":
        parent,child=integer("parent"),integer("child")
        if parent==child or type(cmd["enabled"]) is not bool:
            raise ValueError("parent edge invalid")
        return f"RIG_PARENT {parent} {child} {finite('stiffness',0,1)} {int(cmd['enabled'])}"
    if op=="rig.weight":
        return f"RIG_WEIGHT {integer('vertex')} {finite('weight',0,1)}"
    if op=="rig.weight.inspect":
        return f"RIG_WEIGHT_INSPECT {integer('vertex')}"
    if op=="layer.component.relation":
        first,second=integer("first"),integer("second")
        if first==second:raise ValueError("identical components")
        return f"LAYER_COMPONENT_RELATION {first} {second}"
    if op=="rig.apply":return "RIG_APPLY"
    if op=="rig.inspect":return "RIG_INSPECT"
    if op=="layer.scan":return "LAYER_SCAN"
    if op=="layer.suggest":return "LAYER_SUGGEST"
    if op=="layer.adopt":return "LAYER_ADOPT"
    if op=="layer.label":
        if type(cmd["kind"]) is not str or cmd["kind"] not in ("unknown","hair","skirt","strap"):
            raise ValueError("invalid manually confirmed layer role")
        return "LAYER_LABEL "+cmd["kind"]
    if op=="physics.set":
        return "PHYSICS_SET "+" ".join((finite("gravity",-80,80),finite("wind",-60,60),finite("damping",0,1)))
    if cmd["kind"] not in ("pull","release","shake","settle"):raise ValueError("invalid action")
    return f"ACTION {cmd['kind']} {finite('strength',0,1)}"
def _writer_claim(folder:Path)->int:
    """Exclusive cross-process byte-range lock, OS-released after a crash.

    The lock file is deliberately retained. Its presence is not ownership.
    """
    import sys
    path=folder/"writer.lock"
    try:
        fd=os.open(path,os.O_CREAT|os.O_RDWR,0o600)
        if os.fstat(fd).st_size==0:
            os.write(fd,bytes([0]))
        os.lseek(fd,0,os.SEEK_SET)
        if sys.platform=="win32":
            import msvcrt
            msvcrt.locking(fd,msvcrt.LK_NBLCK,1)
        else:
            import fcntl
            fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        return fd
    except OSError as exc:
        if "fd" in locals():os.close(fd)
        raise RuntimeError("Studio queue busy: writer claim unavailable") from exc

def _writer_release(fd:int)->None:
    import sys
    try:
        os.lseek(fd,0,os.SEEK_SET)
        if sys.platform=="win32":
            import msvcrt
            msvcrt.locking(fd,msvcrt.LK_UNLCK,1)
        else:
            import fcntl
            fcntl.flock(fd,fcntl.LOCK_UN)
    finally:
        os.close(fd)

class AgentBridge:
    def __init__(self,folder:Path,timeout:float=5):
        self.folder=Path(folder);self.timeout=timeout
    def call(self,command:dict)->dict:
        wire=validate_command(command)
        self.folder.mkdir(parents=True,exist_ok=True)
        request=self.folder/"request.txt";response=self.folder/"response.txt"
        writer_fd=_writer_claim(self.folder)
        token=secrets.token_hex(12)
        temporary=self.folder/("request-"+token+".tmp")
        intent=self.folder/"uncertain_outcome"
        acknowledged=False
        try:
            if (request.exists() or response.exists() or intent.exists() or
                    (self.folder/"response.tmp").exists()):
                raise RuntimeError("Studio queue not empty; recovery inspection required")
            # Durable local intent before request publication: a timeout is UNKNOWN_OUTCOME.
            with intent.open("x",encoding="ascii") as marker:
                marker.write(token+"\n")
                marker.flush()
                os.fsync(marker.fileno())
            temporary.write_text(token+" "+wire+"\n",encoding="ascii")
            os.replace(temporary,request)
            deadline=time.monotonic()+self.timeout
            while time.monotonic()<deadline:
                if response.exists():
                    try:
                        with response.open("rb") as handle:
                            frame=handle.read(513)
                        if len(frame)>512:
                            raise RuntimeError("Studio receipt exceeds fixed bound")
                        line=frame.decode("ascii")
                    except (OSError,UnicodeError):
                        line=""
                    if line.startswith(token+" ") and line.endswith("\n") and line.count("\n")==1:
                        fields=line[:-1].removesuffix("\r").split(" ",2)
                        if len(fields)!=3 or fields[1] not in ("OK","ERR") or not fields[2]:
                            raise RuntimeError("Invalid Studio receipt frame")
                        acknowledged=True
                        return {"ok":fields[1]=="OK","receipt":fields[2]}
                time.sleep(.015)
            raise TimeoutError("Native Studio did not acknowledge command")
        finally:
            temporary.unlink(missing_ok=True)
            if request.exists():
                try:
                    line=request.read_text(encoding="ascii")
                    if line.startswith(token+" "):
                        request.unlink(missing_ok=True)
                except (OSError,UnicodeError):
                    pass
            if response.exists():
                try:
                    line=response.read_text(encoding="ascii")
                    if line.startswith(token+" "):
                        response.unlink(missing_ok=True)
                except (OSError,UnicodeError):
                    pass
            # Keep the intent on crash/timeout or incomplete cleanup: no blind replay.
            if acknowledged and not request.exists() and not response.exists():
                intent.unlink(missing_ok=True)
            _writer_release(writer_fd)
