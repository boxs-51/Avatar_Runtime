"""Legacy frame witness from real Studio must never claim G5 APPLIED."""
import pytest
from scripts.studio_agent_api import validate_command
from scripts.studio_agent_tools import AgentStudioTools

class Bridge:
    def __init__(self, receipt):
        self.receipt=receipt
        self.calls=[]
    def call(self, command):
        self.calls.append(dict(command))
        return {"ok":True,"receipt":self.receipt}

def test_valid_local_legacy_frame_metadata_never_sets_g5_applied():
    bridge=Bridge("writer=HOLD frame=LEGACY_ONLY gen=99 rigrev=8 "
                  "bound=8 accepted=8 serial=25 current=1")
    result=AgentStudioTools(bridge).legacy_frame_status()
    assert result["legacy_frame_current"] is True
    assert result["accepted_frame_serial"]==25
    assert result["g5_applied"] is False
    assert result["g5_writer_enabled"] is False
    assert bridge.calls==[{"op":"studio.g5.legacy_frame.status"}]
    assert validate_command(bridge.calls[0])=="G5_LEGACY_FRAME_STATUS"

@pytest.mark.parametrize("response",(
    "writer=ENABLED frame=G5_APPLIED gen=99 rigrev=8 bound=8 accepted=8 serial=25 current=1",
    "writer=HOLD frame=LEGACY_ONLY gen=99 rigrev=8 bound=7 accepted=8 serial=25 current=1",
    "writer=HOLD frame=LEGACY_ONLY gen=99 rigrev=8 bound=8 accepted=8 serial=0 current=1",
    "writer=HOLD frame=LEGACY_ONLY gen=99 rigrev=8 bound=8 accepted=8 serial=25 current=1 model=private",
    "writer=HOLD frame=LEGACY_ONLY gen=99 rigrev=8 bound=0 accepted=0 serial=0 current=2",
))
def test_forbid_overclaim_or_malformed_receipt(response):
    with pytest.raises(RuntimeError):
        AgentStudioTools(Bridge(response)).legacy_frame_status()

def test_stale_legacy_frame_does_not_claim_g5_applied():
    bridge=Bridge("writer=HOLD frame=LEGACY_ONLY gen=99 rigrev=9 "
                  "bound=8 accepted=8 serial=25 current=0")
    r=AgentStudioTools(bridge).legacy_frame_status()
    assert r["legacy_frame_current"] is False
    assert r["g5_applied"] is False
