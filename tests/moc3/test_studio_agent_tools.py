"""Agent-facing API is callable tooling, never an agent identity registry."""
import json
import pytest

from scripts.studio_agent_tools import AgentStudioTools, available_tools


class BridgeStub:
    def __init__(self):
        self.calls = []

    def call(self, command):
        self.calls.append(command.copy())
        return {"ok": True, "receipt": "SYNTHETIC_OK"}


def test_catalog_supports_agent_work_by_function_without_agent_identity():
    groups = available_tools()
    assert "studio" in groups and "rig" in groups and "xpbd" in groups
    assert "studio.status" in groups["studio"]
    assert groups["rig"]["rig.parent"] == ("child", "enabled", "parent", "stiffness")
    assert all("agent_id" not in schema and "identity" not in schema
               for ops in groups.values() for schema in ops.values())


def test_call_uses_existing_local_agent_bridge_and_validation():
    bridge = BridgeStub()
    toolkit = AgentStudioTools(bridge)
    result = toolkit.call("rig.parent", parent=0, child=2, stiffness=0.75, enabled=True)
    assert result == {"ok": True, "receipt": "SYNTHETIC_OK"}
    assert bridge.calls == [{
        "op": "rig.parent", "parent": 0, "child": 2,
        "stiffness": 0.75, "enabled": True
    }]


def test_invalid_operation_never_reaches_bridge():
    bridge = BridgeStub()
    toolkit = AgentStudioTools(bridge)
    with pytest.raises(ValueError):
        toolkit.call("shell.run", cmd="whoami")
    with pytest.raises(ValueError):
        toolkit.call("rig.parent", parent=0, child=0, stiffness=1.0, enabled=True)
    assert not bridge.calls


def test_local_bridge_required_for_execution_but_not_for_catalog():
    assert "studio" in AgentStudioTools().catalog()
    with pytest.raises(RuntimeError):
        AgentStudioTools().call("studio.status")


def test_uncertain_ack_not_retried_by_toolkit():
    class UnknownAck:
        count = 0
        def call(self, command):
            self.count += 1
            raise TimeoutError("ACK not received")
    bridge = UnknownAck()
    with pytest.raises(TimeoutError):
        AgentStudioTools(bridge).call("rig.apply")
    assert bridge.count == 1


def test_v2_local_read_uses_existing_bridge_and_cannot_enable_g5_writer():
    bridge = BridgeStub()
    toolkit = AgentStudioTools(bridge)
    reply = toolkit.read_v2("avatar.capabilities")
    assert reply["state"] == "COMPLETED_READ"
    assert reply["result"]["mutating_commands_enabled"] is False
    assert reply["result"]["native_v2_enabled"] is False
    denied = toolkit.read_v2("draft.commit")
    assert denied["state"] == "REJECTED"
    assert bridge.calls == []


def test_invalid_local_receipt_is_rejected():
    class BadBridge:
        def call(self, command):
            return {"ok": "true", "receipt": "fake"}
    with pytest.raises(RuntimeError):
        AgentStudioTools(BadBridge()).call("studio.status")

def test_reserved_operation_key_cannot_override_selected_tool():
    bridge = BridgeStub()
    with pytest.raises(ValueError):
        AgentStudioTools(bridge).call("studio.status", op="rig.apply")
    assert not bridge.calls
