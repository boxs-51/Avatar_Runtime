"""Windows model-free G5 functional Agent tooling -> Native C11 fixture E2E.

Does not connect to shipping Studio, model files, network or public ABI20.
"""
import ctypes
from pathlib import Path

from scripts.studio_g5_tools import G5DraftTools,G5ToolError

PATH=Path("build/avatar-studio/Release/studio_g5_ipc_fixture.dll")
CODES=("OK","BUSY","REVISION_CONFLICT","STALE_SESSION","STALE_MODEL",
       "INVALID_DRAFT","OUT_OF_BUDGET","ID_CONFLICT","UNKNOWN")
OPS={"draft.begin":1,"draft.anchor.set":2,"draft.validate":3,
     "draft.commit":4,"draft.abort":5,"draft.undo":6,"draft.redo":7}


class NativeFixture:
    is_g5_synthetic_fixture=True

    def __init__(self,epoch=32001):
        if not PATH.is_file():
            raise RuntimeError("G5 fixture DLL has not been built")
        self.native=ctypes.CDLL(str(PATH.resolve()))
        lib=self.native
        u64=ctypes.c_uint64
        lib.g5_fixture_init.argtypes=[u64];lib.g5_fixture_init.restype=ctypes.c_int
        lib.g5_fixture_tool_submit.argtypes=[ctypes.c_uint,u64,u64,u64,u64,u64,
                                              ctypes.c_int,ctypes.c_int,
                                              ctypes.POINTER(u64),ctypes.POINTER(u64)]
        lib.g5_fixture_tool_submit.restype=ctypes.c_int
        lib.g5_fixture_tool_status.argtypes=[u64,u64,ctypes.POINTER(u64),ctypes.c_uint]
        lib.g5_fixture_tool_status.restype=ctypes.c_int
        lib.g5_fixture_tool_receipt.argtypes=[u64,u64,u64,ctypes.POINTER(u64),
                                               ctypes.POINTER(ctypes.c_uint)]
        lib.g5_fixture_tool_receipt.restype=ctypes.c_int
        lib.g5_fixture_tool_mark_unknown_commit.argtypes=[u64,u64,u64,u64,u64]
        lib.g5_fixture_tool_mark_unknown_commit.restype=ctypes.c_int
        lib.g5_fixture_tool_reconcile.argtypes=[u64,u64,u64]
        lib.g5_fixture_tool_reconcile.restype=ctypes.c_int
        assert lib.g5_fixture_init(epoch)==0
        self.epoch=epoch

    def mark_uncertain_commit(self,args):
        """Test-only: transport owner records original post-effect command."""
        return self.native.g5_fixture_tool_mark_unknown_commit(
            args["session_epoch"],args["model_generation"],
            args["expected_revision"],args["draft_id"],args["request_id"])

    def call_g5(self,operation,args):
        lib=self.native
        epoch=args["session_epoch"];generation=args["model_generation"]
        if operation=="draft.reconcile":
            code=lib.g5_fixture_tool_reconcile(epoch,generation,args["request_id"])
            return {"code":CODES[code]}
        if operation=="draft.status":
            output=(ctypes.c_uint64*7)()
            code=lib.g5_fixture_tool_status(epoch,generation,output,7)
            if code:return {"code":CODES[code]}
            return {"code":"OK","document_revision":output[0],
                    "applied_revision":output[1],"has_draft":bool(output[2]),
                    "draft_id":output[3],"base_revision":output[4],
                    "undo_count":output[5],"redo_count":output[6]}
        if operation=="draft.receipt.status":
            revision=ctypes.c_uint64();opcode=ctypes.c_uint()
            code=lib.g5_fixture_tool_receipt(epoch,generation,args["request_id"],
                                               ctypes.byref(revision),ctypes.byref(opcode))
            if code==8:return {"code":"UNKNOWN","outcome":"UNKNOWN"}
            if code:return {"code":CODES[code]}
            return {"code":"OK","outcome":"COMMITTED","document_revision":revision.value,
                    "operation":opcode.value,"request_id":args["request_id"]}
        draft=ctypes.c_uint64()
        revision=ctypes.c_uint64()
        code=lib.g5_fixture_tool_submit(
            OPS[operation],epoch,generation,args.get("expected_revision",0),
            args.get("draft_id",0),args.get("request_id",0),
            args.get("vertex",0),int(args.get("enabled",False)),
            ctypes.byref(draft),ctypes.byref(revision))
        if code:return {"code":CODES[code]}
        result={"code":"OK"}
        if operation=="draft.begin":result["draft_id"]=draft.value
        if operation in ("draft.commit","draft.undo","draft.redo"):
            result["document_revision"]=revision.value
        return result


def main():
    fixture=NativeFixture()
    tools=G5DraftTools(fixture,session_epoch=fixture.epoch,model_generation=55)
    first=tools.status()
    assert first["document_revision"]==first["applied_revision"]==7
    assert first["has_draft"] is False

    result=tools.begin(expected_revision=7)
    draft=result["draft_id"]
    assert draft>0
    assert tools.status()["has_draft"] is True
    tools.anchor_set(draft_id=draft,vertex=2,enabled=True)
    tools.validate(draft_id=draft)
    assert tools.commit(draft_id=draft,expected_revision=7,request_id=701)["document_revision"]==8
    assert tools.status()["applied_revision"]==7 # COMMITTED != APPLIED
    assert tools.status()["has_draft"] is False
    assert tools.receipt_status(request_id=701)["operation"]==4
    assert tools.receipt_status(request_id=999)["outcome"]=="UNKNOWN"

    for execute in (
        lambda:tools.begin(expected_revision=7),
        lambda:tools.undo(expected_revision=7,request_id=702),
        lambda:tools.commit(draft_id=draft,expected_revision=7,request_id=701)):
        try:execute()
        except G5ToolError as exc:
            assert exc.code in ("REVISION_CONFLICT","ID_CONFLICT","INVALID_DRAFT"),exc.code
        else:raise AssertionError("stale transaction accepted")

    assert tools.undo(expected_revision=8,request_id=702)["document_revision"]==9
    assert tools.redo(expected_revision=9,request_id=703)["document_revision"]==10
    assert tools.receipt_status(request_id=703)["operation"]==7
    fresh=tools.begin(expected_revision=10)["draft_id"]
    tools.abort(draft_id=fresh)
    assert tools.status()["document_revision"]==10
    assert tools.status()["has_draft"] is False
    assert tools.status()["applied_revision"]==7
    try:
        G5DraftTools(fixture,session_epoch=123,model_generation=55).status()
    except G5ToolError as exc:assert exc.code=="STALE_SESSION"
    else:raise AssertionError("stale session accepted")
    try:
        G5DraftTools(fixture,session_epoch=fixture.epoch,model_generation=1).status()
    except G5ToolError as exc:assert exc.code=="STALE_MODEL"
    else:raise AssertionError("stale model accepted")
    print("G5_AGENT_TOOLS_NATIVE_PASS workflow=begin-edit-validate-commit-"
          "status-undo-redo-abort no_model_export=1 studio_writer=HOLD")


if __name__=="__main__":
    main()
