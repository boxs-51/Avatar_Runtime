"""G5 Agent work-tools contract; no live Studio writer or private model."""
import pytest
from scripts.studio_g5_tools import G5DraftTools, g5_tool_catalog, G5ToolError, G5UnknownOutcome


class FakeG5:
    is_g5_synthetic_fixture = True

    def __init__(self):
        self.calls = []
        self.replies = []

    def call_g5(self, operation, args):
        self.calls.append((operation, dict(args)))
        return self.replies.pop(0) if self.replies else {"code": "OK", "document_revision": 8}


def make():
    return FakeG5()


def test_g5_catalog_has_functions_not_agent_administration():
    catalog = g5_tool_catalog()
    assert set(catalog) == {"draft"}
    assert set(catalog["draft"]) == {
        "draft.begin", "draft.anchor.set", "draft.validate", "draft.commit",
        "draft.abort", "draft.undo", "draft.redo", "draft.status",
        "draft.receipt.status", "draft.reconcile"}
    assert all("agent_id" not in repr(v).lower() for v in catalog["draft"].values())


def test_default_absent_or_non_fixture_backend_cannot_enable_writer():
    with pytest.raises(RuntimeError):
        G5DraftTools(None, session_epoch=99, model_generation=55)
    class LiveClaim:
        def call_g5(self, *a):
            raise AssertionError("unverified Studio mutation!")
    with pytest.raises(RuntimeError):
        G5DraftTools(LiveClaim(), session_epoch=99, model_generation=55)


def test_begin_edit_validate_commit_exact_native_envelopes():
    bridge = make()
    tools = G5DraftTools(bridge, session_epoch=99, model_generation=55)
    bridge.replies = [{"code":"OK","draft_id":1}, {"code":"OK"}, {"code":"OK"},
                      {"code":"OK","document_revision":8}]
    assert tools.begin(expected_revision=7)["draft_id"] == 1
    assert tools.anchor_set(draft_id=1, vertex=2, enabled=True)["code"] == "OK"
    assert tools.validate(draft_id=1)["code"] == "OK"
    assert tools.commit(draft_id=1,expected_revision=7,request_id=710)["document_revision"] == 8
    assert [op for op, args in bridge.calls] == [
        "draft.begin","draft.anchor.set","draft.validate","draft.commit"]
    assert all(args["session_epoch"] == 99 and args["model_generation"] == 55
               for _,args in bridge.calls)
    assert bridge.calls[-1][1]["request_id"] == 710


def test_undo_redo_and_abort_have_explicit_arguments():
    bridge=make()
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    tools.undo(expected_revision=8,request_id=711)
    tools.redo(expected_revision=9,request_id=712)
    tools.abort(draft_id=4)
    assert [x[0] for x in bridge.calls] == ["draft.undo","draft.redo","draft.abort"]


def test_status_and_unknown_receipt_are_read_only_and_never_retried():
    bridge=make()
    bridge.replies=[{"code":"OK","document_revision":9,"applied_revision":7,
                     "has_draft":False}, {"code":"UNKNOWN","outcome":"UNKNOWN"}]
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    assert tools.status()["applied_revision"] == 7
    assert tools.receipt_status(request_id=999)["outcome"] == "UNKNOWN"
    assert len(bridge.calls) == 2
    assert [v[0] for v in bridge.calls] == ["draft.status","draft.receipt.status"]


def test_unknown_mutation_never_retried():
    bridge=make()
    bridge.replies=[{"code":"UNKNOWN"}]
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    with pytest.raises(G5UnknownOutcome):
        tools.commit(draft_id=1,expected_revision=7,request_id=710)
    assert len(bridge.calls)==1


@pytest.mark.parametrize("action,kwargs",[
    ("begin",{"expected_revision":True}),
    ("begin",{"expected_revision":-1}),
    ("commit",{"draft_id":1,"expected_revision":7,"request_id":0}),
    ("commit",{"draft_id":1,"expected_revision":7,"request_id":True}),
    ("anchor_set",{"draft_id":1,"vertex":2048,"enabled":True}),
    ("anchor_set",{"draft_id":1,"vertex":1,"enabled":1}),
    ("validate",{"draft_id":0}),
    ("undo",{"expected_revision":8,"request_id":-1}),
    ("invoke",{"operation":"agent.register","arguments":{}}),
    ("invoke",{"operation":"draft.begin","arguments":{"expected_revision":7,"op":"draft.commit"}}),
])
def test_invalid_arguments_fail_before_bridge_call(action,kwargs):
    bridge=make()
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    with pytest.raises((ValueError,G5ToolError)):
        getattr(tools,action)(**kwargs)
    assert bridge.calls == []


def test_native_error_not_masked_as_success():
    bridge=make();bridge.replies=[{"code":"REVISION_CONFLICT"}]
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    with pytest.raises(G5ToolError) as e:
        tools.begin(expected_revision=7)
    assert e.value.code=="REVISION_CONFLICT"


def test_malformed_native_response_fails_closed():
    bridge=make();bridge.replies=[{"code":"OK","document_revision":True}]
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    with pytest.raises(G5ToolError):
        tools.undo(expected_revision=8,request_id=701)


def test_timeout_propagates_without_replay():
    class UnknownTransport:
        is_g5_synthetic_fixture=True
        calls=0
        def call_g5(self,op,args):
            self.calls+=1
            raise TimeoutError("synthetic lost ack")
    bridge=UnknownTransport()
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    with pytest.raises(G5UnknownOutcome):
        tools.commit(draft_id=1,expected_revision=7,request_id=222)
    assert bridge.calls==1


def test_private_model_output_cannot_be_returned_as_g5_receipt():
    bridge=make()
    bridge.replies=[{"code":"OK","document_revision":8,"model_path":"private-synthetic"}]
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    with pytest.raises(G5ToolError) as exc:
        tools.undo(expected_revision=7,request_id=710)
    assert exc.value.code=="INVALID_NATIVE_RECEIPT"


def test_foreign_actor_identity_is_not_a_g5_task_argument():
    bridge=make()
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    with pytest.raises(ValueError):
        tools.invoke("draft.begin",{"expected_revision":7,"agent_id":"test-agent"})
    assert bridge.calls==[]

def test_explicit_reconcile_is_a_tool_not_automatic_replay():
    bridge=make();bridge.replies=[{"code":"OK"}]
    tools=G5DraftTools(bridge,session_epoch=99,model_generation=55)
    assert tools.reconcile(request_id=710)=={"code":"OK"}
    assert bridge.calls==[("draft.reconcile",{
        "request_id":710,"session_epoch":99,"model_generation":55})]
    with pytest.raises(ValueError):
        tools.reconcile(request_id=0)
    assert len(bridge.calls)==1
