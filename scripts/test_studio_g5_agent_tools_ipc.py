"""Physical process-bound Agent G5 workflow through two test-only file lanes.

The synthetic Native Rig lives in a child process. No product Studio IPC,
model file, network endpoint or publicly distributable assets are involved.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from scripts.studio_g5_fixture_ipc import FixtureIpcBackend
from scripts.studio_g5_tools import G5DraftTools, G5ToolError, G5UnknownOutcome

ROOT = Path("build/studio-user")
TIMEOUT = 8.0


def wait_for(predicate):
    until = time.monotonic() + TIMEOUT
    while time.monotonic() < until:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("G5 test subprocess timeout")


def drive():
    if os.name != "nt":
        raise RuntimeError("Windows Native test only")
    ROOT.mkdir(parents=True, exist_ok=True)
    home = Path(tempfile.mkdtemp(prefix="g5-agent-ipc-", dir=str(ROOT)))
    for lane in ("mutation", "recovery"):
        (home / lane).mkdir()
    child = subprocess.Popen(
        [sys.executable, "-B", "-m", "scripts.studio_g5_fixture_ipc",
         "--fixture-server", str(home), "720"],
        cwd=str(Path.cwd()), stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        wait_for(lambda: (home / "ready").exists() or child.poll() is not None)
        assert child.poll() is None, "G5 Native fixture did not start"
        epoch = int((home / "ready").read_text(encoding="ascii"))
        assert 0 < epoch < 2**63
        backend = FixtureIpcBackend(home, timeout=0.8)
        tools = G5DraftTools(backend, session_epoch=epoch, model_generation=55)
        assert backend.lane_for("draft.status") == "recovery"
        assert backend.lane_for("draft.commit") == "mutation"
        initial = tools.status()
        assert initial["document_revision"] == initial["applied_revision"] == 7
        draft = tools.begin(expected_revision=7)["draft_id"]
        tools.anchor_set(draft_id=draft, vertex=2, enabled=True)
        tools.validate(draft_id=draft)
        assert tools.commit(draft_id=draft, expected_revision=7,
                            request_id=701)["document_revision"] == 8
        assert tools.status()["applied_revision"] == 7
        assert tools.receipt_status(request_id=701)["operation"] == 4
        assert tools.undo(expected_revision=8, request_id=702)["document_revision"] == 9
        assert tools.redo(expected_revision=9, request_id=703)["document_revision"] == 10
        transient = tools.begin(expected_revision=10)["draft_id"]
        tools.abort(draft_id=transient)
        assert tools.status()["document_revision"] == 10

        # Adversarial cross-lane injection must never mutate the Native owner.
        from scripts.studio_g5_fixture_ipc import _pack, parse_response
        wrong_token = "a" * 24
        wrong_lane = home / "recovery"
        invalid = _pack({"token": wrong_token, "operation": "draft.undo",
                         "args": {"expected_revision": 10, "request_id": 719,
                                  "session_epoch": epoch, "model_generation": 55}})
        (wrong_lane / "request.txt").write_bytes(invalid)
        wait_for(lambda: (wrong_lane / "response.txt").exists())
        refused = parse_response((wrong_lane / "response.txt").read_bytes(),
                                 wrong_token)
        (wrong_lane / "response.txt").unlink()
        assert refused == {"code": "INVALID_DRAFT"}
        assert tools.status()["document_revision"] == 10

        # Post-effect loss: Native committed but its mutation ACK is dropped.
        uncertain = tools.begin(expected_revision=10)["draft_id"]
        tools.anchor_set(draft_id=uncertain, vertex=4, enabled=True)
        tools.validate(draft_id=uncertain)
        try:
            tools.commit(draft_id=uncertain, expected_revision=10, request_id=720)
        except G5UnknownOutcome as exc:
            assert exc.request_id == 720
        else:
            raise AssertionError("deliberately dropped mutation ACK accepted")
        wait_for(lambda: (home / "ack_dropped").exists())
        assert tools.status()["document_revision"] == 11
        assert tools.status()["applied_revision"] == 7
        assert tools.receipt_status(request_id=720)["outcome"] == "COMMITTED"
        assert tools.receipt_status(request_id=999)["outcome"] == "UNKNOWN"
        assert (home / "mutation" / "uncertain_outcome").exists()
        try:
            tools.begin(expected_revision=11)
        except RuntimeError:
            pass
        else:
            raise AssertionError("unresolved mutation intent allowed a retry")
        assert tools.status()["document_revision"] == 11
        assert not (home / "recovery" / "uncertain_outcome").exists()
        # Querying a COMMITTED receipt does not open the mutation queue.
        # Only an explicit operation with the ORIGINAL request ID can.
        try:
            tools.reconcile(request_id=719)
        except RuntimeError:
            pass
        else:
            raise AssertionError("foreign reconciliation cleared quarantine")
        assert (home / "mutation" / "uncertain_outcome").exists()
        assert tools.reconcile(request_id=720) == {"code": "OK"}
        assert not (home / "mutation" / "uncertain_outcome").exists()
        assert tools.status()["document_revision"] == 11
        resumed = tools.begin(expected_revision=11)["draft_id"]
        tools.abort(draft_id=resumed)
        assert tools.status()["document_revision"] == 11

        # Fixture lifecycle control is out-of-band; not an Agent tool.
        (home / "mutation" / "request.txt").write_bytes(b"STOP")
        child.wait(timeout=TIMEOUT)
        assert child.returncode == 0
        print("G5_AGENT_IPC_NATIVE_PASS lanes=2 processes=2 "
              "workflow=begin-edit-validate-commit-undo-redo-abort "
              "lost_ack=quarantined recovery=read_only "
              "reconcile=explicit no_replay=1 model_private=1")
    finally:
        if child.poll() is None:
            child.kill()
        child.communicate(timeout=TIMEOUT)
        shutil.rmtree(home)


if __name__ == "__main__":
    drive()
