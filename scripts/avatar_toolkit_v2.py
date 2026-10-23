"""ADR-002 V2 discovery-only prototype: no Native IPC or model access."""
from __future__ import annotations
import json,re,math
API_VERSION=2
MAX_REQUEST_BYTES=65536
SUPPORTED=("avatar.capabilities","avatar.schema")
LOCAL_READONLY=("studio.local.status","catalog.parameters.page","catalog.parts.page","catalog.parameter.lookup","catalog.part.lookup")
FIELDS=frozenset(("api_version","request_id","session_id",
 "expected_model_generation","expected_edit_revision","operation",
 "arguments","mode"))
ID=re.compile(r"req-[A-Za-z0-9_-]{1,48}\Z",re.ASCII)

class ToolkitV2Error(ValueError):
 def __init__(self,code="INVALID_ARGUMENT"):
  self.code=code
  super().__init__(code)

def _pairs(pairs):
 out={}
 for k,v in pairs:
  if k in out:raise ToolkitV2Error()
  out[k]=v
 return out

def _integer(s):
 if len(s)>19:raise ToolkitV2Error()
 n=int(s)
 if not -(2**63)<=n<2**63:raise ToolkitV2Error()
 return n

def _float(s):
 n=float(s)
 if not math.isfinite(n):raise ToolkitV2Error()
 return n

def _constant(s):
 raise ToolkitV2Error()

def decode_request(data:bytes)->dict:
 if type(data) is not bytes or not data or len(data)>MAX_REQUEST_BYTES:
  raise ToolkitV2Error("OUT_OF_BUDGET")
 try:
  req=json.loads(data.decode("utf-8","strict"),
   object_pairs_hook=_pairs,parse_int=_integer,parse_float=_float,
   parse_constant=_constant)
 except (UnicodeError,ValueError,OverflowError) as exc:
  raise ToolkitV2Error() from exc
 if type(req) is not dict or set(req)!=FIELDS:raise ToolkitV2Error()
 if type(req["api_version"]) is not int or req["api_version"]!=2:
  raise ToolkitV2Error("UNSUPPORTED")
 if type(req["request_id"]) is not str or not ID.fullmatch(req["request_id"]):
  raise ToolkitV2Error()
 if req["session_id"] is not None or req["expected_edit_revision"] is not None:
  raise ToolkitV2Error("UNSUPPORTED")
 gen=req["expected_model_generation"]
 if gen is not None and (type(gen) is not int or not 0<gen<2**63):
  raise ToolkitV2Error("INVALID_ARGUMENT")
 if type(req["mode"]) is not str or req["mode"]!="read":
  raise ToolkitV2Error("UNSUPPORTED")
 if type(req["operation"]) is not str or len(req["operation"])>64:
  raise ToolkitV2Error()
 if type(req["arguments"]) is not dict:raise ToolkitV2Error()
 return req
def _schema(op):
 return {"operation":op,"effect":"read","arguments":
         ({"operation":"supported-opcode"} if op=="avatar.schema" else {}),
         "result_classification":"PUBLIC_SAFE",
         "transport":"offline-discovery-only"}

def _read(req,local_enabled=False):
 op=req["operation"]
 if op not in SUPPORTED:raise ToolkitV2Error("UNKNOWN_OPERATION")
 args=req["arguments"]
 if op=="avatar.capabilities":
  if args:raise ToolkitV2Error()
  return {"supported_operations":list(SUPPORTED)+(list(LOCAL_READONLY) if local_enabled else []),"local_private_read_enabled":local_enabled,"native_v2_enabled":False,
   "mutating_commands_enabled":False,
   "computer_bridge_avatar_group_registered":False,
   "release_gate":"HOLD","transport":"offline-discovery-only",
   "max_request_bytes":MAX_REQUEST_BYTES}
 if set(args)!={"operation"} or type(args["operation"]) is not str:
  raise ToolkitV2Error()
 if args["operation"] in LOCAL_READONLY and local_enabled:
  schema_args=({"start":"int 0..2047","limit":"int 1..4"} if args["operation"].endswith(".page")
                 else {"id":"ASCII model ID 1..59"} if args["operation"].endswith(".lookup") else {})
  return {"operation":args["operation"],"effect":"read",
    "arguments":schema_args,"result_classification":"LOCAL_PRIVATE",
    "transport":"trusted-local-V1-read-adapter",
    "generation_fence":True,"editor_revision_fence":False}
 if args["operation"] not in SUPPORTED:raise ToolkitV2Error("UNSUPPORTED")
 return _schema(args["operation"])

def execute_request(data:bytes,*,local_bridge=None,allow_local_private=False)->str:
 request_id=None
 try:
  req=decode_request(data)
  request_id=req["request_id"]
  if req["operation"] in LOCAL_READONLY:
   if local_bridge is None or allow_local_private is not True:
    raise ToolkitV2Error("PERMISSION_DENIED")
   from scripts.avatar_toolkit_v2_local import execute_local
   result=execute_local(req["operation"],req["arguments"],local_bridge,req["expected_model_generation"])
  else:
   if req["expected_model_generation"] is not None:
    raise ToolkitV2Error("UNSUPPORTED")
   result=_read(req,local_bridge is not None and allow_local_private is True)
  response={"api_version":2,"request_id":request_id,
            "state":"COMPLETED_READ","result":result,"error":None}
 except ToolkitV2Error as exc:
  response={"api_version":2,"request_id":request_id,
            "state":"REJECTED","result":None,"error":{"code":exc.code}}
 return json.dumps(response,ensure_ascii=True,separators=(",",":"))+"\n"
