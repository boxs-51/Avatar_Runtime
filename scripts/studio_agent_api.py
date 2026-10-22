"""Local-only agent command API for Native Studio. No network or model export."""
from __future__ import annotations
import math
import os
from pathlib import Path
import secrets
import time
VERSION="1.0"
def validate_command(cmd:dict)->str:
    if not isinstance(cmd,dict) or not isinstance(cmd.get("op"),str):
        raise ValueError("invalid operation")
    op=cmd["op"]
    fields={
        "rig.select":{"op","drawable"},
        "rig.anchor":{"op","vertex","enabled"},
        "rig.beam":{"op","a","b","enabled"},
        "rig.apply":{"op"},
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
    if op=="rig.select":return f"RIG_SELECT {integer('drawable')}"
    if op=="rig.anchor":
        v=integer("vertex")
        if type(cmd["enabled"]) is not bool:raise ValueError("enabled must be bool")
        return f"RIG_ANCHOR {v} {int(cmd['enabled'])}"
    if op=="rig.beam":
        a,b=integer("a"),integer("b")
        if a==b or type(cmd["enabled"]) is not bool:raise ValueError("invalid beam")
        return f"RIG_BEAM {a} {b} {int(cmd['enabled'])}"
    if op=="rig.apply":return "RIG_APPLY"
    if op=="physics.set":
        return "PHYSICS_SET "+" ".join((finite("gravity",-80,80),finite("wind",-60,60),finite("damping",0,1)))
    if cmd["kind"] not in ("pull","release","shake","settle"):raise ValueError("invalid action")
    return f"ACTION {cmd['kind']} {finite('strength',0,1)}"
class AgentBridge:
    def __init__(self,folder:Path,timeout:float=5):
        self.folder=Path(folder);self.timeout=timeout
    def call(self,command:dict)->dict:
        wire=validate_command(command)
        self.folder.mkdir(parents=True,exist_ok=True)
        request=self.folder/"request.txt";response=self.folder/"response.txt"
        if request.exists():raise RuntimeError("Studio queue busy")
        token=secrets.token_hex(12)
        temporary=self.folder/("request-"+token+".tmp")
        temporary.write_text(token+" "+wire+"\n",encoding="ascii")
        os.replace(temporary,request)
        deadline=time.monotonic()+self.timeout
        try:
            while time.monotonic()<deadline:
                if response.exists():
                    line=response.read_text(encoding="ascii").strip()
                    if line.startswith(token+" "):
                        _,status,*remainder=line.split(" ")
                        return {"ok":status=="OK","receipt":" ".join(remainder)}
                time.sleep(.015)
            raise TimeoutError("Native Studio did not acknowledge command")
        finally:
            request.unlink(missing_ok=True)
            if response.exists():
                line=response.read_text(encoding="ascii").strip()
                if line.startswith(token+" "):response.unlink(missing_ok=True)
