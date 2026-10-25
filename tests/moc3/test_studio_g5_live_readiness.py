"""RED-first: real Studio local G5 readiness must not pretend to be a writer."""
import pytest
from scripts.studio_agent_api import validate_command
from scripts.studio_agent_tools import AgentStudioTools


def test_g5_status_supported_by_existing_local_command_parser():
    assert validate_command({"op":"studio.g5.status"})=="G5_LOCAL_STATUS"


def test_g5_status_returns_local_only_hold_not_native_writer():
    class ReadOnlyBridge:
        calls=[]
        def call(self,cmd):
            self.calls.append(dict(cmd))
            return {"ok":True,"receipt":
                "writer=HOLD txn=OFF frame=UNVERIFIED gen=99 rigrev=8"}
    bridge=ReadOnlyBridge()
    status=AgentStudioTools(bridge).g5_status()
    assert status=={"classification":"LOCAL_PRIVATE","model_generation":99,
      "legacy_rig_revision":8,"g5_writer_enabled":False,
      "g5_transaction_connected":False,"frame_acceptance_verified":False}
    assert bridge.calls==[{"op":"studio.g5.status"}]


@pytest.mark.parametrize("raw",[
 "writer=ENABLED txn=ON frame=ACCEPTED gen=99 rigrev=8",
 "writer=HOLD txn=OFF frame=UNVERIFIED gen=-1 rigrev=8",
 "writer=HOLD txn=OFF frame=UNVERIFIED gen=99 rigrev=8 model=private",
 "writer=HOLD txn=OFF frame=UNVERIFIED gen=99 rigrev=True",
 "writer=HOLD txn=OFF frame=UNVERIFIED gen=9223372036854775808 rigrev=1",
])
def test_fail_closed_on_malformed_or_overclaiming_status(raw):
    class Bridge:
        def call(self,cmd):
            return {"ok":True,"receipt":raw}
    with pytest.raises(RuntimeError):
        AgentStudioTools(Bridge()).g5_status()


def test_g5_readiness_does_not_confuse_missing_model_with_enabled_writer():
    class Bridge:
        def call(self,cmd):
            return {"ok":True,"receipt":
                "writer=HOLD txn=OFF frame=UNVERIFIED gen=0 rigrev=0"}
    state=AgentStudioTools(Bridge()).g5_status()
    assert state["model_generation"]==0
    assert state["legacy_rig_revision"]==0
    assert state["g5_writer_enabled"] is False


def test_g5_status_command_refuses_extra_fields():
    with pytest.raises(ValueError):
        validate_command({"op":"studio.g5.status","draft_id":1})
    with pytest.raises(ValueError):
        validate_command({"op":"studio.g5.status","agent_id":"synthetic"})


def test_g5_status_ack_denied_by_old_studio_is_not_misread_as_connected():
    class OldBridge:
        def call(self,cmd):
            return {"ok":False,"receipt":"rejected"}
    with pytest.raises(RuntimeError):
        AgentStudioTools(OldBridge()).g5_status()
