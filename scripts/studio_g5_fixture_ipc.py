"""Two-lane, two-process G5 Agent-tool IPC *fixture*.

Synthetic Native G5 DLL only. NOT Studio IPC, not an OS security boundary,
not a public MCP/HTTP endpoint, and no model paths/geometry or Agent registry.
All mutation is serialized by the child Native owner and never retried on ACK
loss. Recovery metadata is read-only on a separate filesystem lane.
"""
from __future__ import annotations

import json
import os
import re
import secrets
import sys
import time
from pathlib import Path

from scripts.studio_agent_api import _writer_claim, _writer_release
from scripts.studio_g5_tools import _COMMANDS, _READS, _RESULT, _args, _integer

MAX_FRAME = 512
TOKEN = re.compile(r"[0-9a-f]{24}\Z", re.ASCII)
MAX_WAIT = 4.0


def _unique_pairs(pairs):
    output = {}
    for k, v in pairs:
        if k in output:
            raise ValueError("duplicate IPC key")
        output[k] = v
    return output


def _strict_load(blob):
    if type(blob) is not bytes or len(blob) > MAX_FRAME or not blob:
        raise ValueError("invalid IPC frame")
    try:
        data = json.loads(blob.decode("ascii", "strict"),
                          object_pairs_hook=_unique_pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(ValueError("invalid constant")))
    except (UnicodeError, ValueError) as exc:
        raise ValueError("invalid IPC JSON") from exc
    return data


def parse_request(blob):
    """Validate a finite tool grammar. No user-controlled paths or identities."""
    frame = _strict_load(blob)
    if type(frame) is not dict or set(frame) != {"token", "operation", "args"}:
        raise ValueError("invalid IPC request fields")
    token, operation, values = frame["token"], frame["operation"], frame["args"]
    if type(token) is not str or not TOKEN.fullmatch(token):
        raise ValueError("invalid IPC token")
    if type(operation) is not str or operation not in _COMMANDS:
        raise ValueError("unsupported IPC operation")
    if (type(values) is not dict or
        set(values) != (set(_COMMANDS[operation]) |
                        {"session_epoch", "model_generation"})):
        raise ValueError("invalid IPC arguments")
    _args(operation, {k: v for k, v in values.items()
                      if k not in ("session_epoch", "model_generation")})
    _integer(values["session_epoch"], "session_epoch")
    _integer(values["model_generation"], "model_generation")
    return token, operation, values


def parse_response(blob, token):
    frame = _strict_load(blob)
    if type(frame) is not dict or set(frame) != {"token", "result"}:
        raise ValueError("invalid IPC response fields")
    if type(token) is not str or not TOKEN.fullmatch(token) or frame["token"] != token:
        raise ValueError("response token mismatch")
    result = frame["result"]
    if type(result) is not dict or not set(result) <= _RESULT:
        raise ValueError("invalid IPC result")
    return result


def _pack(data):
    raw = json.dumps(data, separators=(",", ":"), ensure_ascii=True,
                     allow_nan=False).encode("ascii")
    if len(raw) > MAX_FRAME:
        raise ValueError("IPC payload exceeds fixed size")
    return raw


class FixtureIpcBackend:
    """A *test-only* adapter implementing G5DraftTools.call_g5 over two lanes."""

    is_g5_synthetic_fixture = True

    def __init__(self, directory: Path, timeout: float = MAX_WAIT):
        self._root = Path(directory)
        if type(timeout) not in (int, float) or not (0.05 <= timeout <= 30):
            raise ValueError("invalid fixture timeout")
        self._timeout = float(timeout)

    @staticmethod
    def lane_for(operation):
        if operation not in _COMMANDS:
            raise ValueError("unsupported G5 fixture operation")
        return "recovery" if operation in _READS else "mutation"

    def call_g5(self, operation, args):
        # The server independently checks this same bounded grammar.
        token = secrets.token_hex(12)
        blob = _pack({"token": token, "operation": operation, "args": args})
        parse_request(blob)
        lane = self._root / self.lane_for(operation)
        if not lane.is_dir() or lane.is_symlink():
            raise RuntimeError("G5 synthetic IPC lane unavailable")
        lock = _writer_claim(lane)
        request = lane / "request.txt"
        response = lane / "response.txt"
        intent = lane / "uncertain_outcome"
        temporary = lane / ("request-" + token + ".tmp")
        acked = False
        published = False
        try:
            if request.exists() or response.exists():
                raise RuntimeError("G5 fixture queue busy")
            if operation == "draft.reconcile":
                if not intent.is_file():
                    raise RuntimeError("No uncertain original request")
                match = re.fullmatch(r"[0-9a-f]{24} ([0-9]{1,19})\n",
                                     intent.read_text(encoding="ascii"),re.ASCII)
                if not match or int(match.group(1)) != args["request_id"]:
                    raise RuntimeError("Original request ID mismatch")
            elif operation not in _READS:
                if intent.exists():
                    raise RuntimeError("G5 fixture queue quarantined")
                with intent.open("x", encoding="ascii") as marker:
                    marker.write(token + " " + str(args.get("request_id",0)) + "\n")
                    marker.flush()
                    os.fsync(marker.fileno())
            temporary.write_bytes(blob)
            os.replace(temporary, request)
            published = True
            deadline = time.monotonic() + self._timeout
            while time.monotonic() < deadline:
                if response.is_file():
                    raw = response.read_bytes()
                    parsed = parse_response(raw, token)
                    acked = True
                    response.unlink()
                    return parsed
                time.sleep(0.015)
            raise TimeoutError("G5 synthetic IPC ACK uncertain; do not retry")
        finally:
            temporary.unlink(missing_ok=True)
            if request.exists():
                try:
                    payload = request.read_bytes()
                    if parse_request(payload)[0] == token:
                        request.unlink()
                except (OSError, ValueError):
                    pass
            # Never remove the intent after TIMEOUT or invalid/mismatched ACK.
            if (acked and operation not in _READS and not request.exists()
                and (operation != "draft.reconcile" or parsed.get("code") == "OK")):
                intent.unlink(missing_ok=True)
            _writer_release(lock)


def run_server(root: Path, *, drop_request_id: int = 0):
    """Only launched by an explicit synthetic test harness."""
    from scripts.test_studio_g5_agent_tools_native import NativeFixture
    from scripts.studio_g5_tools import _response
    epoch = secrets.randbits(63) or 1
    native = NativeFixture(epoch=epoch)
    mutation, recovery = root / "mutation", root / "recovery"
    assert mutation.is_dir() and recovery.is_dir()
    (root / "ready").write_text(str(epoch), encoding="ascii")
    quarantine = False
    while True:
        handled = False
        for lane, read_only in ((recovery, True), (mutation, False)):
            path = lane / "request.txt"
            if not path.is_file():
                continue
            handled = True
            with path.open("rb") as stream:
                payload = stream.read(MAX_FRAME+1)
            path.unlink()
            if not read_only and payload == b"STOP":
                return 0
            token = ""
            try:
                token, operation, args = parse_request(payload)
                if (operation in _READS) != read_only:
                    raise ValueError("incorrect IPC lane")
                if quarantine and not read_only and operation != "draft.reconcile":
                    result = {"code": "BUSY"}
                else:
                    result = native.call_g5(operation, args)
                    result = _response(operation, result)
                    if operation == "draft.reconcile" and result["code"] == "OK":
                        quarantine = False
                    if (not read_only and operation == "draft.commit" and
                        args.get("request_id") == drop_request_id and drop_request_id and
                        result["code"] == "OK"):
                        # Native owner binds the original committed request first.
                        if native.mark_uncertain_commit(args) != 0:
                            raise RuntimeError("Native receipt/quarantine binding failed")
                        quarantine = True
                        (root / "ack_dropped").write_text("synthetic", encoding="ascii")
                        continue
            except (ValueError, KeyError, TypeError):
                # Fixed non-identifying response; never echo model input/fields.
                result = {"code": "INVALID_DRAFT"}
            reply = _pack({"token": token, "result": result})
            temp = lane / "response.tmp"
            temp.write_bytes(reply)
            os.replace(temp, lane / "response.txt")
        if not handled:
            time.sleep(0.01)


def main():
    if (len(sys.argv) != 4 or sys.argv[1] != "--fixture-server" or
        os.name != "nt"):
        raise SystemExit("G5 fixture subprocess only, not a public service")
    root = Path(sys.argv[2])
    if not root.is_dir() or root.is_symlink():
        raise SystemExit("invalid test fixture root")
    drop = int(sys.argv[3])
    if not 0 <= drop < 2**63:
        raise SystemExit("invalid test fault input")
    return run_server(root, drop_request_id=drop)


if __name__ == "__main__":
    raise SystemExit(main())
