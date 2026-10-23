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
 if len({item["id"] for item in items})!=len(items):
  raise ToolkitV2Error("INVALID_DRAFT")
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


G4_KINDS=("anchor","beam","parent","weight","cross")
G4_META_FIELDS=("drawable","vertices","triangles","islands","used","loose","largest")
G4_RIG_FIELDS=("drawable","vertices","role","anchors","beams","parents","weights","cross")
def _g4_arguments(op,args):
 if type(args) is not dict:raise ToolkitV2Error()
 fields={"drawable"} if op in ("mesh.topology.inspect","rig.local.inspect") else {"drawable","start","limit"}
 if op=="rig.entries.page":fields=fields|{"kind"}
 if set(args)!=fields:raise ToolkitV2Error()
 d=args["drawable"]
 if type(d) is not int or not 0<=d<MAX_ITEMS:raise ToolkitV2Error()
 if op in ("mesh.topology.inspect","rig.local.inspect"):return d,0,0,None
 start,limit=args["start"],args["limit"]
 if type(start) is not int or type(limit) is not int or not 0<=start<MAX_ITEMS or not 1<=limit<=MAX_PAGE:
  raise ToolkitV2Error()
 kind=args.get("kind")
 if op=="rig.entries.page" and (type(kind) is not str or kind not in G4_KINDS):raise ToolkitV2Error()
 return d,start,limit,kind

def _g4_target_vertex_count(bridge,epoch,drawable):
 fields=_parse_receipt(_call(bridge,{"op":"g4.mesh.vertex_count",
                    "generation":epoch,"drawable":drawable}),("drawable","vertices"))
 if _integer(fields["drawable"])!=drawable:raise ToolkitV2Error("STALE_MODEL")
 vertices=_integer(fields["vertices"])
 if not 3<=vertices<=2048:raise ToolkitV2Error("OUT_OF_BUDGET")
 return vertices


def _g4_metadata(bridge,op,epoch,drawable):
 wire="g4.rig.meta" if op.startswith("rig.") else "g4.mesh.meta"
 names=G4_RIG_FIELDS if wire=="g4.rig.meta" else G4_META_FIELDS
 fields=_parse_receipt(_call(bridge,{"op":wire,"generation":epoch,"drawable":drawable}),names)
 values={key:_integer(fields[key]) for key in names}
 if values["drawable"]!=drawable:raise ToolkitV2Error("STALE_MODEL")
 if not 3<=values["vertices"]<=2048:raise ToolkitV2Error("OUT_OF_BUDGET")
 if wire=="g4.mesh.meta":
  used=values["used"]
  islands=values["islands"]
  largest=values["largest"]
  triangles=values["triangles"]
  if (not 1<=triangles<=21845 or
      not 3<=used<=values["vertices"] or
      not 0<=values["loose"]<=values["vertices"] or
      used+values["loose"]!=values["vertices"] or
      not 1<=islands<=min(triangles,used//3) or
      not max(3,(used+islands-1)//islands)<=largest<=used-3*(islands-1)):
   raise ToolkitV2Error("INVALID_DRAFT")
 else:
  vertices=values["vertices"]
  max_beam_pairs=vertices*(vertices-1)//2
  limits=(("anchors",min(64,vertices)),
          ("beams",min(128,max_beam_pairs)),
          ("parents",min(64,vertices-1)),
          ("weights",min(256,vertices)),
          ("cross",16))
  if values["role"]>3 or any(values[k]>limit for k,limit in limits):
   raise ToolkitV2Error("INVALID_DRAFT")
 return values

def _g4_item(bridge,op,epoch,d,index,kind,meta,drawable_count):
 base={"generation":epoch,"drawable":d,"index":index}
 if op=="mesh.vertices.page":
  fields=_parse_receipt(_call(bridge,dict(base,op="g4.mesh.vertex")),("drawable","index","x","y"))
  if _integer(fields["drawable"])!=d or _integer(fields["index"])!=index:raise ToolkitV2Error("STALE_MODEL")
  return {"index":index,"x":_floating(fields["x"]),"y":_floating(fields["y"])}
 if op=="mesh.triangles.page":
  fields=_parse_receipt(_call(bridge,dict(base,op="g4.mesh.triangle")),("drawable","index","a","b","c"))
  if _integer(fields["drawable"])!=d or _integer(fields["index"])!=index:raise ToolkitV2Error("STALE_MODEL")
  vertices=[_integer(fields[k]) for k in ("a","b","c")]
  if len(set(vertices))!=3 or any(v>=meta["vertices"] for v in vertices):raise ToolkitV2Error("INVALID_DRAFT")
  return {"index":index,"vertices":vertices}
 spec={
  "anchor":("kind","index","vertex"),
  "beam":("kind","index","a","b"),
  "parent":("kind","index","parent","child","stiffness"),
  "weight":("kind","index","vertex","weight"),
  "cross":("kind","index","parent","target","child","stiffness")}
 fields=_parse_receipt(_call(bridge,dict(base,op="g4.rig.entry",kind=kind)),spec[kind])
 if fields["kind"]!=kind or _integer(fields["index"])!=index:raise ToolkitV2Error("STALE_MODEL")
 item={"kind":kind,"index":index}
 for key in spec[kind][2:]:
  value=_floating(fields[key]) if key in ("stiffness","weight") else _integer(fields[key])
  if (key in ("vertex","a","b","parent") or (kind=="parent" and key=="child")) and value>=meta["vertices"]:raise ToolkitV2Error("INVALID_DRAFT")
  if key=="weight" and not 0<=value<=1:raise ToolkitV2Error("INVALID_DRAFT")
  if key=="stiffness" and not 0<=value<=1:raise ToolkitV2Error("INVALID_DRAFT")
  item[key]=value
 if kind=="beam" and item["a"]==item["b"]:raise ToolkitV2Error("INVALID_DRAFT")
 if kind=="parent" and item["parent"]==item["child"]:
  raise ToolkitV2Error("INVALID_DRAFT")
 if kind=="cross":
  target=item["target"]
  if target==d or target>=drawable_count:
   raise ToolkitV2Error("INVALID_DRAFT")
  target_vertices=_g4_target_vertex_count(bridge,epoch,target)
  if item["child"]>=target_vertices:
   raise ToolkitV2Error("INVALID_DRAFT")
 return item

def _g4_rig_revision(bridge,epoch,drawable):
 raw=_parse_receipt(_call(bridge,{"op":"g4.rig.revision",
                                  "generation":epoch,"drawable":drawable}),
                    ("revision",))["revision"]
 # Legacy index parsing is limited to 31 bits; revisions require 63 bits.
 if re.fullmatch(r"(?:0|[1-9][0-9]{0,18})",raw,re.ASCII) is None:
  raise ToolkitV2Error("INVALID_DRAFT")
 number=int(raw)
 if number>=2**63:raise ToolkitV2Error("INVALID_DRAFT")
 return number


def read_geometry(bridge,op,args,expected=None):
 d,start,limit,kind=_g4_arguments(op,args)
 state=read_status(bridge)
 epoch=state["model_generation"]
 if expected is not None and expected!=epoch:raise ToolkitV2Error("STALE_MODEL")
 if d>=state["counts"]["drawables"]:raise ToolkitV2Error("INVALID_ARGUMENT")
 revision=_g4_rig_revision(bridge,epoch,d) if op.startswith("rig.") else None
 meta=_g4_metadata(bridge,op,epoch,d)
 total=(meta["vertices"] if op=="mesh.vertices.page" else meta["triangles"] if op=="mesh.triangles.page" else meta[kind+"s" if kind!="cross" else "cross"] if kind else 0)
 items=[]
 if op.endswith(".page"):
  if start>=total:raise ToolkitV2Error("INVALID_ARGUMENT")
  for i in range(start,min(total,start+limit)):
   items.append(_g4_item(bridge,op,epoch,d,i,kind,meta,state["counts"]["drawables"]))
 if revision is not None and _g4_rig_revision(bridge,epoch,d)!=revision:
  raise ToolkitV2Error("STALE_REVISION")
 after=read_status(bridge)
 if after["model_generation"]!=epoch or after["counts"]!=state["counts"]:raise ToolkitV2Error("STALE_MODEL")
 common={"classification":"LOCAL_PRIVATE","model_generation":epoch,"drawable":d,
         "editor_revision_available":revision is not None,
         "consistency":"model_epoch_and_rig_revision_guarded" if revision is not None
                       else "model_epoch_guarded_editor_unfenced"}
 if revision is not None:common["editor_revision"]=revision
 if op=="mesh.topology.inspect":
  return dict(common,vertices=meta["vertices"],triangles=meta["triangles"],
              topology={k:meta[k] for k in ("islands","used","loose","largest")})
 if op=="rig.local.inspect":return dict(common,rig={k:meta[k] for k in G4_RIG_FIELDS if k!="drawable"})
 return dict(common,items=items,total=total,next_start=start+len(items) if start+len(items)<total else None)

def execute_local(op,args,bridge,expected=None):
 if op in ("mesh.topology.inspect","mesh.vertices.page","mesh.triangles.page","rig.local.inspect","rig.entries.page"):
  return read_geometry(bridge,op,args,expected)
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
