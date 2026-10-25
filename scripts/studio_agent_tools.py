"""Local-only callable Studio tools for an already connected Agent.

This is an ergonomic wrapper around the EXISTING validated V1 bridge and
read-only V2 adapter. It does NOT register agents, publish HTTP/MCP endpoints,
grant access, export model assets, or activate G5 Native transactions.
"""
from __future__ import annotations

import json
import secrets
import re

from scripts.studio_agent_api import operation_fields, validate_command
from scripts.avatar_toolkit_v2 import execute_request

MAX_LOCAL_RECEIPT = 512


def available_tools() -> dict[str, dict[str, tuple[str, ...]]]:
    """Discover bounded operation names and required argument keys.

    A public API means a documented software interface, not model disclosure.
    No model-dependent data or Agent identities are included in discovery.
    """
    by_group: dict[str, dict[str, tuple[str, ...]]] = {}
    for operation, fields in sorted(operation_fields().items()):
        group = operation.split(".", 1)[0]
        by_group.setdefault(group, {})[operation] = tuple(
            sorted(field for field in fields if field != "op"))
    return by_group


class AgentStudioTools:
    """Use with an explicitly supplied existing trusted-local AgentBridge."""

    def __init__(self, bridge=None):
        self._bridge = bridge

    def catalog(self) -> dict[str, dict[str, tuple[str, ...]]]:
        return available_tools()

    def call(self, operation: str, **arguments):
        """Call one existing V1 Studio action; never retry unknown outcomes."""
        if self._bridge is None:
            raise RuntimeError("Studio local bridge not attached")
        if type(operation) is not str or "op" in arguments:
            raise ValueError("invalid Studio operation")
        command = {"op": operation, **arguments}
        # The native-bound V1 validator remains the sole grammar source.
        validate_command(command)
        result = self._bridge.call(command.copy())
        if (type(result) is not dict or
            set(result) != {"ok", "receipt"} or
            type(result["ok"]) is not bool or
            type(result["receipt"]) is not str or
            not result["receipt"] or
            len(result["receipt"]) > MAX_LOCAL_RECEIPT or
            any(char in result["receipt"] for char in "\r\n\x00")):
            raise RuntimeError("Invalid local Studio receipt")
        return result

    def recover_v1(self, request_token: str) -> dict:
        """Read-only product V1 receipt query. UNKNOWN never authorizes retry."""
        if type(request_token) is not str or re.fullmatch(
            r"[0-9a-f]{24}",request_token,re.ASCII) is None:
            raise ValueError("invalid Studio recovery token")
        if self._bridge is None or not callable(getattr(self._bridge,"recover",None)):
            raise RuntimeError("Local Studio recovery bridge unavailable")
        result=self._bridge.recover(request_token)
        if (type(result) is not dict or
            (result != {"state":"UNKNOWN"} and result not in
             ({"state":"FOUND","ok":True},{"state":"FOUND","ok":False}))):
            raise RuntimeError("Invalid Studio recovery result")
        return result

    def reconcile_v1(self, request_token: str) -> dict:
        """Explicitly unquarantine after a matching real Studio receipt."""
        if type(request_token) is not str or re.fullmatch(
            r"[0-9a-f]{24}",request_token,re.ASCII) is None:
            raise ValueError("invalid Studio recovery token")
        if self._bridge is None or not callable(getattr(self._bridge,"reconcile",None)):
            raise RuntimeError("Local Studio reconciliation unavailable")
        result=self._bridge.reconcile(request_token)
        if (type(result) is not dict or result not in
            ({"state":"RECONCILED","ok":True},{"state":"RECONCILED","ok":False})):
            raise RuntimeError("Invalid Studio reconciliation result")
        return result

    def legacy_frame_status(self) -> dict:
        """Local XPBD/GPU witness for legacy Rig only; G5 APPLIED is unverified."""
        answer=self.call("studio.g5.legacy_frame.status")
        if not answer["ok"]:
            raise RuntimeError("Studio legacy frame status unavailable")
        match=re.fullmatch(
            r"writer=HOLD frame=LEGACY_ONLY gen=([0-9]{1,19}) "
            r"rigrev=([0-9]{1,19}) bound=([0-9]{1,19}) "
            r"accepted=([0-9]{1,19}) serial=([0-9]{1,19}) current=([01])",
            answer["receipt"],re.ASCII)
        if match is None:
            raise RuntimeError("Invalid legacy frame witness")
        gen,rig,bound,accepted,serial,current=map(int,match.groups())
        if max(gen,rig,bound,accepted,serial)>=2**63:
            raise RuntimeError("Legacy frame witness out of range")
        if (current and (not serial or not bound or
                         bound!=rig or accepted!=rig)) or (
                         not current and serial==0 and accepted!=0):
            raise RuntimeError("Inconsistent legacy frame witness")
        return {"classification":"LOCAL_PRIVATE","g5_writer_enabled":False,
                "g5_applied":False,"model_generation":gen,
                "legacy_rig_revision":rig,"bound_rig_revision":bound,
                "accepted_rig_revision":accepted,"accepted_frame_serial":serial,
                "legacy_frame_current":bool(current)}

    def g5_status(self) -> dict:
        """Read actual Studio HOLD gates, not a fabricated G5 document state."""
        answer = self.call("studio.g5.status")
        if answer["ok"] is not True:
            raise RuntimeError("G5 status not available in this Studio")
        line = answer["receipt"]
        result = re.fullmatch(
            r"writer=HOLD txn=OFF frame=UNVERIFIED gen=([0-9]{1,19}) "
            r"rigrev=([0-9]{1,19})", line, re.ASCII)
        if result is None:
            raise RuntimeError("Invalid G5 status receipt")
        generation, rig_revision = (int(item) for item in result.groups())
        if not (0 <= generation < 2**63 and 0 <= rig_revision < 2**63):
            raise RuntimeError("Out of range G5 status")
        return {"classification": "LOCAL_PRIVATE",
                "model_generation": generation,
                "legacy_rig_revision": rig_revision,
                "g5_writer_enabled": False,
                "g5_transaction_connected": False,
                "frame_acceptance_verified": False}

    def read_v2(self, operation: str, arguments=None, *, expected_generation=None):
        """Use existing V2 read adapter; never enable G5 stage/write operations."""
        if self._bridge is None:
            raise RuntimeError("Studio local bridge not attached")
        if arguments is None:
            arguments = {}
        envelope = {
            "api_version": 2,
            "request_id": "req-tool-" + secrets.token_hex(8),
            "session_id": None,
            "expected_model_generation": expected_generation,
            "expected_edit_revision": None,
            "operation": operation,
            "arguments": arguments,
            "mode": "read",
        }
        # No network, paths, identity registry or retry.
        raw = execute_request(
            json.dumps(envelope, ensure_ascii=True).encode("utf-8"),
            local_bridge=self._bridge,
            allow_local_private=True,
        )
        return json.loads(raw)
