"""Bounded local-only V1 read adapter for the experimental V2 envelope.

This module is called with an existing, explicitly supplied trusted-local
AgentBridge. It cannot open models, derive IPC paths or mutate state.
"""
from __future__ import annotations
import math,re
from scripts.avatar_toolkit_v2 import ToolkitV2Error

STATUS_FIELDS=("frame","playing","expression","tab","xpbd","params","parts","drawables")
MAX_PAGE=4
MODEL_ID_HEX=re.compile(r"(?:[0-9a-f]{2}){1,96}\Z",re.ASCII)

def _valid_model_id(value):
 if type(value) is not str or "/" in value or "\\" in value:
  return False
 if any(ord(c)<32 or 127<=ord(c)<=159 for c in value):
  return False
 try:
  encoded=value.encode("utf-8",errors="strict")
 except UnicodeError:
  return False
 return 1<=len(encoded)<=96

def _decode_model_id(raw):
 if type(raw) is not str or MODEL_ID_HEX.fullmatch(raw) is None:
  raise ToolkitV2Error("INVALID_DRAFT")
 try:
  text=bytes.fromhex(raw).decode("utf-8",errors="strict")
 except (UnicodeError,ValueError) as exc:
  raise ToolkitV2Error("INVALID_DRAFT") from exc
 if not _valid_model_id(text):raise ToolkitV2Error("INVALID_DRAFT")
 return text

MAX_ITEMS=2048
NUM=re.compile(r"[0-9]{1,10}\Z")
FLOAT=re.compile(r"-?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d{1,3})?\Z")

def _parse_receipt(raw,fields):
 if type(raw) is not str or len(raw)>240 or "\n" in raw or "\r" in raw:
  raise ToolkitV2Error("INVALID_DRAFT")
 values=raw.split(" ")
 if len(values)!=len(fields):
  raise ToolkitV2Error("INVALID_DRAFT")
 parts={}
 for item,key in zip(values,fields):
  if "=" not in item:raise ToolkitV2Error("INVALID_DRAFT")
  k,v=item.split("=",1)
  if k!=key or not v:raise ToolkitV2Error("INVALID_DRAFT")
  parts[k]=v
 return parts

def _integer(value):
 if not NUM.fullmatch(value):raise ToolkitV2Error("INVALID_DRAFT")
 number=int(value)
 if number>2**31-1:raise ToolkitV2Error("INVALID_DRAFT")
 return number

def _floating(value):
 if not FLOAT.fullmatch(value):raise ToolkitV2Error("INVALID_DRAFT")
 number=float(value)
 if not math.isfinite(number):raise ToolkitV2Error("INVALID_DRAFT")
 return number

def _call(bridge,command):
 try:answer=bridge.call(command)
 except TimeoutError as exc:raise ToolkitV2Error("TIMEOUT_UNKNOWN") from exc
 except (RuntimeError,OSError,ValueError) as exc:
  raise ToolkitV2Error("BUSY") from exc
 if type(answer) is not dict or answer.get("ok") is not True:
  raise ToolkitV2Error("INVALID_DRAFT")
 return answer.get("receipt")

def _read_epoch(bridge):
 parsed=_parse_receipt(_call(bridge,{"op":"studio.epoch"}),("generation",))
 value=parsed["generation"]
 if not re.fullmatch(r"[0-9]{1,19}",value):
  raise ToolkitV2Error("INVALID_DRAFT")
 generation=int(value)
 if not 0<generation<2**63:raise ToolkitV2Error("INVALID_DRAFT")
 return generation

def read_status(bridge):
 before=_read_epoch(bridge)
 parts=_parse_receipt(_call(bridge,{"op":"studio.status"}),STATUS_FIELDS)
 if _read_epoch(bridge)!=before:raise ToolkitV2Error("STALE_MODEL")
 data={k:_integer(parts[k]) for k in STATUS_FIELDS}
 if data["playing"] not in (0,1) or data["xpbd"] not in (0,1):
  raise ToolkitV2Error("INVALID_DRAFT")
 return {"frame":data["frame"],"playing":bool(data["playing"]),
   "xpbd":bool(data["xpbd"]),"expression":data["expression"],
   "tab":data["tab"],"counts":{
    "params":data["params"],"parts":data["parts"],"drawables":data["drawables"]},
   "classification":"LOCAL_PRIVATE",
   "backend":"legacy-v1-read-only","generation_available":True,
   "model_generation":before,"editor_revision_available":False}

def read_page(bridge,operation,args,expected=None):
 if type(args) is not dict or set(args)!={"start","limit"}:
  raise ToolkitV2Error()
 start,limit=args["start"],args["limit"]
 if type(start) is not int or type(limit) is not int or (
   not 0<=start<MAX_ITEMS or not 1<=limit<=MAX_PAGE):
  raise ToolkitV2Error()
 state=read_status(bridge)
 if expected is not None and state["model_generation"]!=expected:raise ToolkitV2Error("STALE_MODEL")
 field="params" if operation=="catalog.parameters.page" else "parts"
 total=state["counts"][field]
 if total>MAX_ITEMS:raise ToolkitV2Error("OUT_OF_BUDGET")
 if start>=total:raise ToolkitV2Error("INVALID_ARGUMENT")
 items=[]
 for index in range(start,min(total,start+limit)):
  if operation=="catalog.parameters.page":
   try:raw=_call(bridge,{"op":"param.inspect.at","index":index,
                             "generation":state["model_generation"]})
   except ToolkitV2Error as exc:
    if exc.code=="INVALID_DRAFT":raise ToolkitV2Error("STALE_MODEL") from exc
    raise
   values=_parse_receipt(raw,("index","value","low","high","pin"))
   if _integer(values["index"])!=index:raise ToolkitV2Error("INVALID_DRAFT")
   value,low,high=(_floating(values[k]) for k in ("value","low","high"))
   pin=_integer(values["pin"])
   if pin not in (0,1) or low>high:raise ToolkitV2Error("INVALID_DRAFT")
   items.append({"index":index,"value":value,"low":low,
                "high":high,"pinned":bool(pin)})
  else:
   try:raw=_call(bridge,{"op":"part.inspect.at","index":index,
                            "generation":state["model_generation"]})
   except ToolkitV2Error as exc:
    if exc.code=="INVALID_DRAFT":raise ToolkitV2Error("STALE_MODEL") from exc
    raise
   values=_parse_receipt(raw,("index","opacity"))
   if _integer(values["index"])!=index:raise ToolkitV2Error("INVALID_DRAFT")
   opacity=_floating(values["opacity"])
   if not 0<=opacity<=1:raise ToolkitV2Error("INVALID_DRAFT")
   items.append({"index":index,"opacity":opacity})
  try:
   id_op="param.id.at" if operation=="catalog.parameters.page" else "part.id.at"
   ident=_parse_receipt(_call(bridge,{"op":id_op,"generation":state["model_generation"],
                     "index":index}),("index","idhex"))
   if _integer(ident["index"])!=index:
    raise ToolkitV2Error("INVALID_DRAFT")
   items[-1]["id"]=_decode_model_id(ident["idhex"])
  except ToolkitV2Error as exc:
   if exc.code=="INVALID_DRAFT":raise ToolkitV2Error("STALE_MODEL") from exc
   raise
 after=read_status(bridge)
 if after["counts"]!=state["counts"] or after["model_generation"]!=state["model_generation"]:raise ToolkitV2Error("STALE_MODEL")
 next_index=start+len(items)
 return {"classification":"LOCAL_PRIVATE","backend":"legacy-v1-read-only",
         "items":items,"next_start":next_index if next_index<total else None,
         "total":total,"generation_available":True,"model_generation":state["model_generation"],
         "editor_revision_available":False,"consistency":"model_epoch_guarded_editor_unfenced"}

def read_lookup(bridge,op,args,expected=None):
 if type(args) is not dict or set(args)!={"id"} or type(args["id"]) is not str:
  raise ToolkitV2Error()
 if not _valid_model_id(args["id"]):raise ToolkitV2Error()
 state=read_status(bridge)
 epoch=state["model_generation"]
 if expected is not None and expected!=epoch:raise ToolkitV2Error("STALE_MODEL")
 is_parameter=op=="catalog.parameter.lookup"
 prefix="param" if is_parameter else "part"
 total=state["counts"]["params" if is_parameter else "parts"]
 raw=_call(bridge,{"op":prefix+".find.at","generation":epoch,"id":args["id"]})
 idx=_integer(_parse_receipt(raw,("index",))["index"])
 if idx>=total:raise ToolkitV2Error("INVALID_DRAFT")
 found=_parse_receipt(_call(bridge,{"op":prefix+".id.at",
                           "generation":epoch,"index":idx}),("index","idhex"))
 if _integer(found["index"])!=idx or _decode_model_id(found["idhex"])!=args["id"]:
  raise ToolkitV2Error("STALE_MODEL")
 after=read_status(bridge)
 if after["model_generation"]!=epoch or after["counts"]!=state["counts"]:
  raise ToolkitV2Error("STALE_MODEL")
 return {"index":idx,"id":args["id"],"model_generation":epoch,
   "classification":"LOCAL_PRIVATE","generation_available":True,
   "editor_revision_available":False}

def execute_local(op,args,bridge,expected=None):
 if op=="studio.local.status":
  if args:raise ToolkitV2Error()
  state=read_status(bridge)
  if expected is not None and state["model_generation"]!=expected:
   raise ToolkitV2Error("STALE_MODEL")
  return state
 if op in ("catalog.parameters.page","catalog.parts.page"):
  return read_page(bridge,op,args,expected)
 if op in ("catalog.parameter.lookup","catalog.part.lookup"):
  return read_lookup(bridge,op,args,expected)
 raise ToolkitV2Error("UNKNOWN_OPERATION")
