"""Guard against accidentally exposing unqualified G5 writes in Studio/V2.

G5 native transaction tests are local-only experiments until GUI/Agent routing,
recovery IPC and frame acceptance are independently verified.
"""
import json
from pathlib import Path

from scripts.avatar_toolkit_v2 import execute_request


def test_g5_core_not_linked_to_live_studio_target():
    cmake=Path("tools/avatar-studio/CMakeLists.txt").read_text(encoding="utf-8")
    live=cmake.split("add_executable(avatar_studio",1)[1].split(
        "add_dependencies(avatar_studio",1)[0]
    assert "studio_g5_bus.c" not in live
    assert "studio_g5_txn.c" not in live
    assert "studio_g5_recovery_wire.c" not in live
    native=Path("tools/avatar-studio/studio.c").read_text(encoding="utf-8")
    assert '#include "studio_g5_bus.h"' not in native
    assert '#include "studio_g5_txn.h"' not in native


def test_g5_advertised_mutating_capability_is_disabled():
    req={
        "api_version":2,"request_id":"req-g5-guard",
        "session_id":None,"expected_model_generation":None,
        "expected_edit_revision":None,
        "operation":"avatar.capabilities",
        "arguments":{},"mode":"read",
    }
    reply=json.loads(execute_request(json.dumps(req).encode("utf-8")))
    assert reply["state"]=="COMPLETED_READ"
    assert reply["result"]["mutating_commands_enabled"] is False
    assert reply["result"]["release_gate"]=="HOLD"


def test_g5_mutating_envelope_still_fails_closed_without_native_call():
    class Trap:
        def call(self, command):
            raise AssertionError("Native IPC mutation was reached")
    req={
        "api_version":2,"request_id":"req-g5-denied",
        "session_id":"session-synthetic",
        "expected_model_generation":55,"expected_edit_revision":7,
        "operation":"draft.begin",
        "arguments":{"drawable":0},"mode":"stage",
    }
    reply=json.loads(execute_request(
        json.dumps(req).encode("utf-8"),
        local_bridge=Trap(),allow_local_private=True))
    assert reply["state"]=="REJECTED"
    assert reply["result"] is None
    assert reply["error"]["code"]=="UNSUPPORTED"
