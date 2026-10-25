"""G5 functional task tools for model-free Native testing ONLY.

Not a public writer, Agent registration API, or model export.
G5 Studio writer remains HOLD until physical owner, recovery and frame gates.
"""
from __future__ import annotations

MAX_INT=(1<<63)-1
_COMMANDS={
 "draft.begin":("expected_revision",),
 "draft.anchor.set":("draft_id","vertex","enabled"),
 "draft.validate":("draft_id",),
 "draft.commit":("draft_id","expected_revision","request_id"),
 "draft.abort":("draft_id",),
 "draft.undo":("expected_revision","request_id"),
 "draft.redo":("expected_revision","request_id"),
 "draft.status":(),
 "draft.receipt.status":("request_id",),
 "draft.reconcile":("request_id",),
}
_CODES=frozenset(("OK","BUSY","REVISION_CONFLICT","STALE_SESSION",
 "STALE_MODEL","INVALID_DRAFT","OUT_OF_BUDGET","ID_CONFLICT","UNKNOWN"))
_READS=frozenset(("draft.status","draft.receipt.status"))
_RESULT=frozenset(("code","draft_id","document_revision","applied_revision",
 "base_revision","has_draft","undo_count","redo_count","outcome",
 "operation","request_id"))
class G5ToolError(RuntimeError):
 def __init__(self,code):
  self.code=code
  super().__init__("G5 tool "+code)
class G5UnknownOutcome(G5ToolError):
 def __init__(self,request_id=None):
  super().__init__("UNKNOWN_OUTCOME")
  self.request_id=request_id

def g5_tool_catalog():
 """Stable task-oriented schemas, no Agent account or access management."""
 return {"draft":dict(_COMMANDS)}

def _integer(value,name,allow_zero=False):
 if type(value) is not int or not (0 if allow_zero else 1)<=value<=MAX_INT:
  raise ValueError("invalid "+name)

def _args(operation,arguments):
 if type(operation) is not str or operation not in _COMMANDS:
  raise ValueError("unsupported G5 operation")
 if type(arguments) is not dict or set(arguments)!=set(_COMMANDS[operation]):
  raise ValueError("invalid G5 arguments")
 for key,value in arguments.items():
  if key=="enabled":
   if type(value) is not bool:raise ValueError("invalid enabled")
  elif key=="vertex":
   if type(value) is not int or not 0<=value<2048:raise ValueError("invalid vertex")
  else:_integer(value,key,allow_zero=(key=="expected_revision"))
 return dict(arguments)

def _response(operation,reply):
 if type(reply) is not dict or not set(reply)<=_RESULT:
  raise G5ToolError("INVALID_NATIVE_RECEIPT")
 code=reply.get("code")
 if type(code) is not str or code not in _CODES:
  raise G5ToolError("INVALID_NATIVE_RECEIPT")
 for key,val in reply.items():
  if key=="code":continue
  if key=="outcome":
   if type(val) is not str or val not in ("COMMITTED","UNKNOWN"):
    raise G5ToolError("INVALID_NATIVE_RECEIPT")
  elif key=="has_draft":
   if type(val) is not bool:raise G5ToolError("INVALID_NATIVE_RECEIPT")
  elif type(val) is not int or not 0<=val<=MAX_INT:
   raise G5ToolError("INVALID_NATIVE_RECEIPT")
 if code=="OK":
  if operation=="draft.begin" and reply.get("draft_id",0)<1:
   raise G5ToolError("INVALID_NATIVE_RECEIPT")
  if operation in ("draft.commit","draft.undo","draft.redo") and reply.get("document_revision",0)<1:
   raise G5ToolError("INVALID_NATIVE_RECEIPT")
  if operation=="draft.status" and (reply.get("document_revision",0)<1 or
     reply.get("applied_revision",0)<1 or "has_draft" not in reply):
   raise G5ToolError("INVALID_NATIVE_RECEIPT")
 return dict(reply)

class G5DraftTools:
 """Task API with an explicitly supplied model-free Native test backend."""
 def __init__(self,backend,*,session_epoch,model_generation):
  _integer(session_epoch,"session_epoch")
  _integer(model_generation,"model_generation")
  if (backend is None or
      getattr(backend,"is_g5_synthetic_fixture",False) is not True or
      not callable(getattr(backend,"call_g5",None))):
   raise RuntimeError("G5 Native Studio writer HOLD; test fixture required")
  self._backend=backend
  self._session=session_epoch
  self._generation=model_generation

 def invoke(self,operation,arguments):
  args=_args(operation,arguments)
  values=dict(args,session_epoch=self._session,model_generation=self._generation)
  try:reply=self._backend.call_g5(operation,values)
  except TimeoutError as exc:
   raise G5UnknownOutcome(args.get("request_id")) from exc
  result=_response(operation,reply)
  if result["code"]=="UNKNOWN" and operation in _READS:return result
  if result["code"]=="UNKNOWN":raise G5UnknownOutcome(args.get("request_id"))
  if result["code"]!="OK":raise G5ToolError(result["code"])
  return result

 def begin(self,*,expected_revision):
  return self.invoke("draft.begin",{"expected_revision":expected_revision})
 def anchor_set(self,*,draft_id,vertex,enabled):
  return self.invoke("draft.anchor.set",{"draft_id":draft_id,"vertex":vertex,"enabled":enabled})
 def validate(self,*,draft_id):
  return self.invoke("draft.validate",{"draft_id":draft_id})
 def commit(self,*,draft_id,expected_revision,request_id):
  return self.invoke("draft.commit",{"draft_id":draft_id,"expected_revision":expected_revision,"request_id":request_id})
 def abort(self,*,draft_id):
  return self.invoke("draft.abort",{"draft_id":draft_id})
 def undo(self,*,expected_revision,request_id):
  return self.invoke("draft.undo",{"expected_revision":expected_revision,"request_id":request_id})
 def redo(self,*,expected_revision,request_id):
  return self.invoke("draft.redo",{"expected_revision":expected_revision,"request_id":request_id})
 def status(self):
  return self.invoke("draft.status",{})
 def receipt_status(self,*,request_id):
  return self.invoke("draft.receipt.status",{"request_id":request_id})
 def reconcile(self,*,request_id):
  """Explicit recovery after verifying receipt; NEVER retry original write."""
  return self.invoke("draft.reconcile",{"request_id":request_id})
