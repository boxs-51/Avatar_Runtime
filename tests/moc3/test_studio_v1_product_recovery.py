"""RED-first: real product V1 receipt recovery is read-only and no replay."""
import pytest
from pathlib import Path
from scripts.studio_agent_api import AgentBridge, StudioUnknownOutcome
from scripts.studio_agent_tools import AgentStudioTools


def test_timeout_exposes_original_token_without_replaying(tmp_path):
    bridge=AgentBridge(tmp_path,timeout=0.05)
    with pytest.raises(StudioUnknownOutcome) as e:
        bridge.call({"op":"studio.g5.status"})
    token=e.value.request_token
    assert len(token)==24 and all(ch in "0123456789abcdef" for ch in token)
    assert (tmp_path/"uncertain_outcome").read_text("ascii").strip()==token
    assert not (tmp_path/"request.txt").exists()


def test_recovery_unknown_on_unconnected_live_studio(tmp_path):
    bridge=AgentBridge(tmp_path,timeout=0.05)
    with pytest.raises(TimeoutError):
        bridge.recover("a"*24)
    assert not (tmp_path/"uncertain_outcome").exists()


def test_local_agent_tool_recovery_method_delegates_without_registration():
    class Bridge:
        def __init__(self):self.calls=[]
        def recover(self,token):
            self.calls.append(token)
            return {"state":"UNKNOWN"}
    b=Bridge()
    assert AgentStudioTools(b).recover_v1("a"*24)=={"state":"UNKNOWN"}
    assert b.calls==["a"*24]


def test_reconcile_never_clears_undetermined_outcome(tmp_path):
    bridge=AgentBridge(tmp_path,timeout=0.05)
    token="a"*24
    (tmp_path/"uncertain_outcome").write_text(token+"\n",encoding="ascii")
    bridge.recover=lambda _: {"state":"UNKNOWN"}
    with pytest.raises(RuntimeError):
        bridge.reconcile(token)
    assert (tmp_path/"uncertain_outcome").exists()


def test_reconcile_requires_matching_client_intent_and_native_receipt(tmp_path):
    bridge=AgentBridge(tmp_path,timeout=0.05)
    token="a"*24
    other="b"*24
    (tmp_path/"uncertain_outcome").write_text(token+"\n",encoding="ascii")
    bridge.recover=lambda _: {"state":"FOUND","ok":True}
    with pytest.raises(RuntimeError):
        bridge.reconcile(other)
    assert (tmp_path/"uncertain_outcome").exists()
    assert bridge.reconcile(token)=={"state":"RECONCILED","ok":True}
    assert not (tmp_path/"uncertain_outcome").exists()


def test_reconcile_cannot_clear_unpublished_or_pending_ack(tmp_path):
    bridge=AgentBridge(tmp_path,timeout=0.05)
    token="a"*24
    (tmp_path/"uncertain_outcome").write_text(token+"\n",encoding="ascii")
    (tmp_path/"response.tmp").write_text("incomplete",encoding="ascii")
    bridge.recover=lambda _: {"state":"FOUND","ok":True}
    with pytest.raises(RuntimeError):
        bridge.reconcile(token)
    assert (tmp_path/"uncertain_outcome").exists()

def test_agent_tools_reconcile_v1_delegates_explicitly():
    class Bridge:
        def __init__(self):self.calls=[]
        def reconcile(self, token):
            self.calls.append(token)
            return {"state":"RECONCILED","ok":True}
    bridge=Bridge()
    assert AgentStudioTools(bridge).reconcile_v1("a"*24)=={
        "state":"RECONCILED","ok":True}
    assert bridge.calls==["a"*24]


def test_agent_tools_refuse_invalid_reconciliation_status():
    class Bridge:
        def reconcile(self, token):
            return {"state":"RECONCILED","ok":"true"}
    with pytest.raises(RuntimeError):
        AgentStudioTools(Bridge()).reconcile_v1("a"*24)
